/**
 * dsh-windhawk-status — Host half.
 *
 * Publishes one DSH session's live state as newline-delimited JSON over local
 * TCP, in the same wire format the OpenCode plugin uses, so the Windhawk mod
 * only has to learn one protocol. One message per session, keyed by an
 * instanceId that carries the session id:
 *
 *     <host>:dsh:<pid>:session:<sessionId>
 *
 * The mod renders one floating window per instance and hides it again after a
 * configurable idle timeout.
 *
 * Two channels, both local-only:
 *
 *   status  (plugin -> 127.0.0.1:19290)  fire-and-forget NDJSON pushes
 *   control (mod    -> 127.0.0.1:19289)  {"action":"focus","sessionId":...}
 *
 * A focus request is parked in memory; the Client half polls
 * GET /api-dsh-status/pending, calls uiWorkspace.openSession, and reports back
 * with POST /api-dsh-status/focus-done. The mod itself raises the Desktop
 * window, because only a window-owning process can do that.
 *
 * Ports and timings come from the environment so the same package works in any
 * profile:
 *
 *   DSH_STATUS_PORT          19290
 *   DSH_STATUS_CONTROL_PORT  19289
 *   DSH_STATUS_HEARTBEAT_MS  5000
 *   DSH_STATUS_FOCUS_TTL_MS  15000
 *
 * 19290 rather than the OpenCode plugin's 19288 on purpose: the OpenCode
 * taskbar mod binds 19288 exclusively, so sharing it would mean only one of the
 * two mods ever gets the port. Point OPENCODE_STATUS_PORT at 19290 instead if
 * you want OpenCode sessions in floating windows too.
 */

import net from 'node:net'
import os from 'node:os'
import { basename } from 'node:path'

export const name = 'dsh-windhawk-status'

// Every dependency is optional: a headless profile without webServer still
// publishes status, and a profile without the workspace UI still renders.
export const inject = []

const PROTOCOL = 'opencode-status'
const PROTOCOL_VERSION = 2
const SOURCE = 'dsh-status'
const APP_LABEL = 'DSH'
const INSTANCE_ID = `${os.hostname()}:dsh:${process.pid}`
const MAX_TRACKED_ATTEMPTS = 64

function envInt(key, fallback, min, max) {
  const parsed = Number.parseInt(process.env[key] ?? '', 10)
  if (!Number.isInteger(parsed) || parsed < min || parsed > max) return fallback
  return parsed
}

const HOST = process.env.DSH_STATUS_HOST || '127.0.0.1'
const STATUS_PORT = envInt('DSH_STATUS_PORT', 19290, 1, 65535)
const CONTROL_PORT = envInt('DSH_STATUS_CONTROL_PORT', 19289, 1, 65535)
const HEARTBEAT_MS = envInt('DSH_STATUS_HEARTBEAT_MS', 5000, 500, 600000)
const FOCUS_TTL_MS = envInt('DSH_STATUS_FOCUS_TTL_MS', 15000, 1000, 600000)

function finite(value, fallback = 0) {
  return typeof value === 'number' && Number.isFinite(value) ? value : fallback
}

function createSession(id) {
  return {
    id,
    title: '',
    cwd: '',
    parentId: '',
    origin: '',
    status: 'idle',
    statusText: '等待输入',
    activity: '',
    indicator: 'none',
    approval: '',
    waiting: '',
    tool: '',
    hasRun: false,
    hasText: false,
    hasReasoning: false,
    runStartedAt: 0,
    lastActivityAt: 0,
    updatedAt: 0,
    tokens: { input: 0, output: 0, reasoning: 0, cacheRead: 0, cacheWrite: 0, total: 0 },
    attempts: new Map(),
    provider: '',
    model: '',
    contextUsed: 0,
    contextLimit: 0,
    goal: null,
    goalCheckedAt: 0,
    /** Latest `todo_write` list: what {task} and {progress} are really about. */
    todos: null,
    signature: '',
  }
}

/** "等待中" only until the assistant produces anything visible. */
function workingStatusText(session) {
  if (session.tool) return session.tool
  if (session.hasText) return '正在回复'
  if (session.hasReasoning) return '思考中'
  return '等待中'
}

function beginRun(session, now) {
  session.hasText = false
  session.hasReasoning = false
  session.tool = ''
  session.approval = ''
  session.waiting = ''
  if (!session.runStartedAt) session.runStartedAt = now
}

function applyWorking(session) {
  if (session.approval) return
  session.status = 'working'
  session.indicator = 'working'
  session.statusText = workingStatusText(session)
}

function applyIdle(session) {
  session.tool = ''
  session.approval = ''
  session.waiting = ''
  session.runStartedAt = 0
  if (session.status === 'error' || session.status === 'aborted') return
  session.status = 'idle'
  session.indicator = 'none'
  session.statusText = session.hasRun ? '已完成' : '等待输入'
}

function applyWaiting(session, kind) {
  session.approval = kind
  session.waiting = kind
  session.status = 'waiting'
  session.indicator = 'approval'
  session.statusText = kind === 'question' ? '等待回答' : '等待审批'
}

function applyMessageTokens(session, usage) {
  if (!usage || typeof usage !== 'object') return
  const entry = {
    input: finite(usage.inputTokens),
    output: finite(usage.outputTokens),
    reasoning: finite(usage.reasoningTokens),
    cacheRead: finite(usage.cacheReadTokens),
    cacheWrite: finite(usage.cacheWriteTokens),
  }
  entry.total = finite(usage.totalTokens, entry.input + entry.output + entry.reasoning)
  return entry
}

function foldAttempts(session) {
  const aggregate = { input: 0, output: 0, reasoning: 0, cacheRead: 0, cacheWrite: 0, total: 0 }
  for (const entry of session.attempts.values()) {
    aggregate.input += entry.input
    aggregate.output += entry.output
    aggregate.reasoning += entry.reasoning
    aggregate.cacheRead += entry.cacheRead
    aggregate.cacheWrite += entry.cacheWrite
    aggregate.total += entry.total
  }
  session.tokens = aggregate
}

/**
 * Parse one `todo_write` argument string into a task list.
 *
 * The tool takes the COMPLETE list every call, so the newest one always wins —
 * there is no need to merge. Anything malformed is ignored rather than cleared,
 * so a bad payload cannot blank out a task list that was already showing.
 */
function applyTodos(session, rawArguments) {
  if (typeof rawArguments !== 'string' || !rawArguments.trim()) return
  let parsed = null
  try {
    parsed = JSON.parse(rawArguments)
  } catch {
    return
  }
  if (!Array.isArray(parsed?.todos)) return
  const todos = []
  for (const todo of parsed.todos) {
    const content = typeof todo?.content === 'string' ? todo.content.trim() : ''
    if (!content) continue
    todos.push({ content, status: typeof todo.status === 'string' ? todo.status : 'pending' })
  }
  session.todos = todos.length ? todos : null
}

/**
 * {task} and {progress}: the todo list first, the goal round counter second.
 *
 * `todo_write` exists to "plan multi-step work and show progress", so it is the
 * honest source for a task line. A goal is a much coarser thing — a long-running
 * objective with a round cap — and most sessions never have one, which is why
 * reading only the goal left both placeholders permanently empty.
 */
function goalProgress(session) {
  const todos = session.todos
  if (Array.isArray(todos) && todos.length) {
    const total = todos.length
    let completed = 0
    let active = ''
    for (const todo of todos) {
      if (todo.status === 'completed') completed++
      else if (!active && todo.status === 'in_progress') active = todo.content
    }
    if (!active) {
      const pending = todos.find((todo) => todo.status !== 'completed')
      active = pending ? pending.content : ''
    }
    return { current: completed, completed, total, label: `${completed}/${total}`, task: active }
  }

  const goal = session.goal
  if (!goal) return { current: 0, completed: 0, total: 0, label: '', task: '' }
  const total = finite(goal.maxGoalRounds)
  const current = finite(goal.roundsStarted)
  const label = total > 0 ? `${current}/${total}` : ''
  return {
    current,
    completed: current,
    total,
    label,
    task: typeof goal.objective === 'string' ? goal.objective : '',
  }
}

/** The newest `todo_write` call in a session log, for adoption after a reload. */
function todosFromLog(events) {
  if (!Array.isArray(events)) return null
  for (let index = events.length - 1; index >= 0; index--) {
    const event = events[index]
    if (event?.type !== 'tool/call') continue
    const data = event.data
    if (data?.name !== 'todo_write') continue
    const holder = { todos: null }
    applyTodos(holder, data.arguments)
    return holder.todos
  }
  return null
}

function snapshotTitle(raw) {
  if (!raw) return ''
  for (const key of ['title', 'text', 'value', 'name']) {
    const value = raw[key]
    if (typeof value === 'string' && value.trim()) return value.trim()
  }
  return ''
}

function errorText(error) {
  if (!error) return ''
  const message = typeof error.message === 'string' ? error.message : ''
  const code = typeof error.code === 'string' ? error.code : ''
  const text = (message || code || '会话出错').trim()
  return text.length > 160 ? `${text.slice(0, 160)}…` : text
}

export function apply(ctx, config = {}) {
  const log = (...args) => {
    try {
      ctx.logger?.info?.(`[dsh-windhawk-status] ${args.join(' ')}`)
    } catch {}
  }
  const warn = (...args) => {
    try {
      ctx.logger?.warn?.(`[dsh-windhawk-status] ${args.join(' ')}`)
    } catch {}
  }

  const sessions = new Map()
  // Display labels for DSH's activity categories, fetched from the Client half
  // (which reads DSH's locale service). Empty until the page reports them, and
  // the payload falls back to the raw tool id in the meantime.
  let activityLabels = {}
  // Diagnostics. `dump` on the control port prints these, which is the only way
  // to tell "no events arrive" from "events arrive but the send gate drops them".
  const stats = {
    track: 0,
    dirty: 0,
    gatedNoRun: 0,
    gatedSame: 0,
    sent: 0,
    sendOk: 0,
    sendFail: 0,
    lastEventAt: 0,
    startedAt: Date.now(),
    events: new Map(),
  }

  /**
   * Register a listener and remember that it fired. "The plugin is silent" has
   * two very different causes — the event never arrived, or it arrived and the
   * send gate dropped it — and only the per-event counters tell them apart.
   */
  const listen = (event, handler) => {
    ctx.on(event, (...args) => {
      const seen = (stats.events.get(event) ?? 0) + 1
      stats.events.set(event, seen)
      stats.lastEventAt = Date.now()
      if (seen === 1) log(`first ${event} received`)
      return handler(...args)
    })
  }
  let connected = false
  let pendingFocus = null
  let disposed = false
  let publishTimer = null
  let lastDiagnosticAt = 0
  const dirty = new Set()

  const sessionQuery = () => ctx.get('sessionQuery')
  const tokenMeter = () => ctx.get('tokenMeter')
  const sessionsStore = () => ctx.get('sessions')
  const goals = () => ctx.get('goals')
  const agents = () => ctx.get('agents')

  function getSession(id) {
    if (!id) return null
    let session = sessions.get(id)
    if (!session) {
      session = createSession(String(id))
      sessions.set(id, session)
    }
    return session
  }

  /**
   * The record for one id, created on demand. Events carry ids, and a session
   * can predate this plugin — after a hot reload or a fresh enable, the running
   * sessions are already live and will never re-emit `api-session/added`. So
   * every event path heals the record instead of dropping the event.
   */
  function trackSession(id) {
    if (!id) return null
    stats.track++
    stats.lastEventAt = Date.now()
    const key = String(id)
    const isNew = !sessions.has(key)
    const session = getSession(key)
    if (isNew) refreshTitle(session)
    return session
  }

  /** Children get no card; `api-session/added` uses the same rule. */
  function isChildHeader(header) {
    return Boolean(header?.parentSession) || header?.origin === 'subagent'
  }

  /**
   * What a session log says about a session we just adopted.
   *
   * `api-session/status` only fires on a *change* of running state. A session
   * that is already mid-turn when this plugin loads never produces that toggle,
   * so nothing would ever mark it as running and its widget would never appear —
   * which is exactly what happens after restarting DSH during a long turn. The
   * session log still knows: the newest turn boundary says whether it is open.
   *
   * The same read recovers the task list, which otherwise stays empty until the
   * next `todo_write` — the live stream is the only other place it arrives.
   */
  function observeLog(session) {
    const query = sessionQuery()
    if (typeof query?.readSession !== 'function') return Promise.resolve(null)
    return Promise.resolve(query.readSession(session.id))
      .then((snapshot) => {
        const events = snapshot?.events
        if (!Array.isArray(events)) return null
        let running = false
        for (let index = events.length - 1; index >= 0; index--) {
          const type = events[index]?.type
          if (type === 'turn/end') break
          if (type === 'turn/start') {
            running = true
            break
          }
        }
        return { running, todos: todosFromLog(events) }
      })
      .catch(() => null)
  }

  /**
   * Adopt the sessions that are already live. Ids and cwd only — `hasRun` stays
   * false, so nothing appears on screen until that session actually works.
   */
  function seedLiveSessions() {
    const store = sessionsStore()
    if (typeof store?.list !== 'function') return
    let live
    try {
      live = store.list()
    } catch {
      return
    }
    if (!Array.isArray(live)) return
    let seeded = 0
    for (const entry of live) {
      const id = entry?.id
      if (!id) continue
      if (isChildHeader(entry.header)) continue
      const session = getSession(String(id))
      if (typeof entry.header?.cwd === 'string' && entry.header.cwd) {
        session.cwd = entry.header.cwd
      }
      if (!session.title) refreshTitle(session)
      // One log read per adopted session recovers both facts the live stream
      // would otherwise have to re-teach us: whether a turn is still open, and
      // the task list behind {task} / {progress}.
      const target = session
      void observeLog(target).then((observed) => {
        if (disposed || !observed) return
        if (observed.todos) target.todos = observed.todos
        // A session already mid-turn gets published now, so the widget is there
        // immediately instead of waiting for the next running-state toggle.
        if (observed.running && !target.hasRun) {
          target.hasRun = true
          if (!target.runStartedAt) target.runStartedAt = Date.now()
          applyWorking(target)
          refreshGoal(target)
          target.lastActivityAt = target.lastActivityAt || Date.now()
        }
        markDirty(target)
      })
      // Publish immediately so the mod knows the instance exists, and so the
      // async title/context lookups have time to land before a card appears.
      markDirty(session)
      seeded++
    }
    if (seeded) log(`adopted ${seeded} live session(s)`)
  }

  // ---------------------------------------------------------------- metadata

  function refreshTitle(session) {
    const query = sessionQuery()
    if (!query?.readTitle) return
    Promise.resolve(query.readTitle(session.id))
      .then((raw) => {
        const title = snapshotTitle(raw)
        if (title && title !== session.title) {
          session.title = title
          session.updatedAt = Date.now()
          markDirty(session)
        }
      })
      .catch(() => {})
  }

  function refreshContext(session) {
    const store = sessionsStore()
    const meter = tokenMeter()
    if (!store?.get) return
    let instance = null
    try {
      instance = store.get(session.id)
    } catch {
      return
    }
    if (!instance) return
    try {
      const request = instance.requestContext?.()
      if (request) {
        if (typeof request.provider === 'string' && request.provider) session.provider = request.provider
        if (typeof request.model === 'string' && request.model) session.model = request.model
        if (Number.isFinite(request.contextWindow)) session.contextLimit = request.contextWindow
      }
    } catch {}
    if (!meter?.measure) return
    try {
      const measurement = meter.measure(instance)
      if (measurement && Number.isFinite(measurement.totalTokens)) {
        session.contextUsed = measurement.totalTokens
      }
    } catch {}
  }

  function refreshGoal(session) {
    const service = goals()
    const registry = agents()
    if (!service?.get) return
    // goals.get() validates that the argument IS the registry's live instance
    // and throws for anything else. A synthetic `{ id }` object therefore always
    // failed, which left {progress} empty for every session.
    const agent = typeof registry?.get === 'function' ? registry.get(session.id) : undefined
    if (!agent) {
      session.goal = null
      return
    }
    try {
      const view = service.get(agent)
      session.goal = view && typeof view === 'object' ? view : null
    } catch {
      session.goal = null
    }
  }

  // ----------------------------------------------------------------- publish

  /**
   * DSH's own tool → activity category mapping, so the wording matches the chat
   * UI exactly. This is classification logic, not a translation table: the text
   * for each category arrives from the Client half, which reads it out of DSH's
   * locale service. Nothing here hardcodes a language.
   */
  function activityCategory(name) {
    if (!name) return 'tools'
    if (name === 'read') return 'read'
    if (name === 'read_image') return 'readImage'
    if (name === 'grep' || name === 'glob' || name.endsWith('_inspect')) return 'search'
    if (name === 'write') return 'write'
    if (name === 'edit' || name === 'apply_patch') return 'edit'
    if (
      ['bash', 'pwsh', 'exec_command', 'write_stdin'].includes(name) ||
      name.startsWith('terminal_')
    ) {
      return 'commands'
    }
    if (name === 'run_code') return 'code'
    if (name === 'web_search') return 'webSearch'
    if (name === 'web_fetch') return 'webFetch'
    if (name === 'subagent' || name.startsWith('subagent_')) return 'subagents'
    if (['todo_write', 'create_goal', 'update_goal', 'get_goal'].includes(name)) return 'plan'
    if (name === 'ask_user_question' || name === 'request_user_input') return 'questions'
    return 'tools'
  }

  /** The label the Client half fetched for one activity category, if any. */
  function activityLabel(name) {
    if (!name) return ''
    return activityLabels[`stepProcess.${activityCategory(name)}`] ?? ''
  }

  function buildPayload(session, type) {
    const progress = goalProgress(session)
    const contextLimit = session.contextLimit
    const contextUsed = session.contextUsed
    // `tool` is the live tool; `activity` remembers the last one after it ends.
    const activityId = session.tool || session.activity
    // Display text, so the mod never has to know a tool name.
    const activityText = activityLabel(activityId)
    const statusText =
      session.status === 'working'
        ? activityText || session.statusText
        : session.status === 'idle' && session.hasRun
          ? activityLabels['turnProcess.worked'] || session.statusText
          : session.status === 'error'
            ? activityLabels['turnProcess.failed'] || session.statusText
            : session.statusText
    return {
      protocol: PROTOCOL,
      version: PROTOCOL_VERSION,
      type,
      source: SOURCE,
      app: APP_LABEL,
      // Top level on purpose: the mod reads `openable` off the root object to
      // decide whether a click jumps to a conversation or opens a terminal.
      openable: true,
      instanceId: `${INSTANCE_ID}:session:${session.id}`,
      pid: process.pid,
      host: os.hostname(),
      timestamp: Date.now(),
      session: {
        id: session.id,
        name: session.title || basename(session.cwd || '') || 'DSH 会话',
        directory: session.cwd,
        status: session.status,
        statusText,
        // Ids are for matching; the texts are for display.
        statusId: session.status === 'working' && activityId
          ? activityCategory(activityId)
          : session.status,
        activity: activityText || activityId,
        activityId,
        indicator: session.indicator,
        approval: session.approval,
        waitingForOutput: session.status === 'working' && !session.hasText &&
          !session.hasReasoning && !session.tool,
        hasRun: session.hasRun,
        lastActivityAt: session.lastActivityAt,
        runStartedAt: session.runStartedAt,
        progress,
        model: { providerId: session.provider, modelId: session.model },
        tokens: {
          input: session.tokens.input,
          output: session.tokens.output,
          reasoning: session.tokens.reasoning,
          cacheRead: session.tokens.cacheRead,
          cacheWrite: session.tokens.cacheWrite,
          total: session.tokens.total,
          contextUsed,
          contextLimit,
          contextPercent: contextLimit > 0 ? Math.min(100, (contextUsed / contextLimit) * 100) : 0,
        },
      },
    }
  }

  function signatureOf(payload) {
    const session = payload.session
    if (!session) return `${payload.instanceId}|${payload.type}`
    return [
      payload.type, session.status, session.statusText, session.statusId,
      session.activity, session.activityId, session.indicator,
      session.approval, session.name, session.directory, session.runStartedAt,
      session.lastActivityAt, session.tokens.total, session.tokens.input,
      session.tokens.output, session.tokens.contextUsed, session.tokens.contextLimit,
      session.progress.label, session.progress.task, session.model.providerId,
      session.model.modelId, session.hasRun,
    ].join('|')
  }

  function send(payload) {
    return new Promise((resolve) => {
      let socket
      let settled = false
      const finish = (ok) => {
        if (settled) return
        settled = true
        connected = ok
        if (ok) stats.sendOk++
        else stats.sendFail++
        try {
          socket?.destroy()
        } catch {}
        resolve(ok)
      }
      try {
        socket = net.createConnection({ host: HOST, port: STATUS_PORT })
      } catch {
        finish(false)
        return
      }
      socket.unref?.()
      socket.setTimeout(750)
      socket.once('error', () => finish(false))
      socket.once('timeout', () => finish(false))
      socket.once('close', () => finish(false))
      socket.once('connect', () => {
        try {
          socket.end(`${JSON.stringify(payload)}\n`, () => finish(true))
        } catch {
          finish(false)
        }
      })
    })
  }

  function publishSession(session, type) {
    // The mod renders every instance it receives, so "only show a session once
    // it has run" has to be enforced here: a session that has never worked is
    // not published at all, and starts publishing when its first turn begins.
    if (!session.hasRun) {
      stats.gatedNoRun++
      return Promise.resolve(false)
    }
    const payload = buildPayload(session, type)
    const signature = signatureOf(payload)
    if (type === 'status' && signature === session.signature) {
      stats.gatedSame++
      return Promise.resolve(false)
    }
    stats.sent++
    session.signature = signature
    return send(payload)
  }

  function publishRemove(sessionId) {
    const payload = {
      protocol: PROTOCOL,
      version: PROTOCOL_VERSION,
      type: 'remove',
      source: SOURCE,
      app: APP_LABEL,
      instanceId: `${INSTANCE_ID}:session:${sessionId}`,
      pid: process.pid,
      host: os.hostname(),
      timestamp: Date.now(),
      session: null,
    }
    return send(payload)
  }

  function markDirty(session) {
    if (disposed) return
    stats.dirty++
    dirty.add(session.id)
    if (publishTimer) return
    publishTimer = setTimeout(() => {
      publishTimer = null
      const ids = [...dirty]
      dirty.clear()
      for (const id of ids) {
        const session = sessions.get(id)
        if (session) void publishSession(session, 'status')
      }
    }, 40)
    publishTimer.unref?.()
  }

  async function heartbeat() {
    if (disposed) return
    for (const session of sessions.values()) {
      // A goal's round counter changes without any session event, so refresh it
      // here rather than only on status transitions.
      if (session.hasRun) refreshGoal(session)
      await publishSession(session, 'heartbeat')
    }
  }

  async function removeSession(sessionId) {
    const session = sessions.get(sessionId)
    sessions.delete(sessionId)
    if (!session) return
    await publishRemove(sessionId)
  }

  // ------------------------------------------------------------------ events

  function touch(session, now = Date.now()) {
    session.updatedAt = now
    session.lastActivityAt = now
    markDirty(session)
  }

  function isChild(summary) {
    return Boolean(summary?.parentSessionId) || summary?.origin === 'subagent'
  }

  function onSessionSummary(summary) {
    if (!summary?.sessionId) return
    if (isChild(summary)) {
      if (sessions.has(String(summary.sessionId))) void removeSession(String(summary.sessionId))
      return
    }
    const session = getSession(summary.sessionId)
    if (typeof summary.cwd === 'string' && summary.cwd) session.cwd = summary.cwd
    if (typeof summary.origin === 'string') session.origin = summary.origin
    session.parentId = summary.parentSessionId ? String(summary.parentSessionId) : ''
    if (summary.running) {
      session.hasRun = true
      if (!session.runStartedAt) session.runStartedAt = Date.now()
      applyWorking(session)
    } else if (session.status === 'working') {
      applyIdle(session)
    }
    if (!session.title) refreshTitle(session)
    session.updatedAt = Date.now()
    markDirty(session)
  }

  listen('api-session/added', (summary) => {
    try {
      onSessionSummary(summary)
    } catch (error) {
      warn('api-session/added failed', String(error))
    }
  })

  listen('api-session/status', (sessionId, running) => {
    try {
      if (!sessionId) return
      const session = trackSession(sessionId)
      if (!session) return
      const now = Date.now()
      if (running) {
        session.hasRun = true
        if (!session.runStartedAt) session.runStartedAt = now
        session.status = 'working'
        session.indicator = 'working'
        session.statusText = workingStatusText(session)
      } else {
        applyIdle(session)
      }
      session.lastActivityAt = now
      touch(session, now)
      refreshContext(session)
      refreshGoal(session)
      markDirty(session)
    } catch (error) {
      warn('api-session/status failed', String(error))
    }
  })

  listen('api-session/activity', (sessionId, updatedAt) => {
    try {
      const session = trackSession(sessionId)
      if (!session) return
      const now = Date.now()
      session.lastActivityAt = finite(updatedAt, now)
      touch(session, now)
    } catch (error) {
      warn('api-session/activity failed', String(error))
    }
  })

  listen('api-session/error', (sessionId, message) => {
    try {
      const session = trackSession(sessionId)
      if (!session) return
      session.status = 'error'
      session.indicator = 'error'
      session.statusText = typeof message === 'string' && message ? message : '会话出错'
      session.tool = ''
      session.approval = ''
      touch(session)
    } catch (error) {
      warn('api-session/error failed', String(error))
    }
  })

  listen('api-session/removed', (sessionId) => {
    void removeSession(String(sessionId)).catch(() => {})
  })

  listen('session/disposed', (session) => {
    try {
      const id = session?.id
      if (id) void removeSession(String(id)).catch(() => {})
    } catch {}
  })

  listen('agent/status', (payload) => {
    try {
      const id = payload?.agent?.id
      if (!id) return
      const session = trackSession(id)
      if (!session) return
      const now = Date.now()
      if (payload.status === 'running') {
        session.hasRun = true
        if (!session.runStartedAt) session.runStartedAt = now
        session.status = 'working'
        session.indicator = session.approval ? 'approval' : 'working'
        session.statusText = workingStatusText(session)
      } else {
        applyIdle(session)
        refreshContext(session)
        refreshGoal(session)
      }
      touch(session, now)
    } catch (error) {
      warn('agent/status failed', String(error))
    }
  })

  listen('agent/assistant-stream', (payload) => {
    try {
      const id = payload?.agent?.id
      const frame = payload?.frame
      if (!id || !frame) return
      const session = trackSession(id)
      if (!session) return
      const now = Date.now()
      session.hasRun = true
      session.lastActivityAt = now

      if (frame.type === 'start') {
        if (!session.runStartedAt) session.runStartedAt = now
        session.status = 'working'
        session.indicator = session.approval ? 'approval' : 'working'
        session.statusText = workingStatusText(session)
        touch(session, now)
        return
      }

      if (frame.type === 'end') {
        if (frame.outcome?.kind === 'abandoned') {
          session.status = 'aborted'
          session.indicator = 'none'
          session.statusText = '已中断'
          session.tool = ''
          session.approval = ''
          session.runStartedAt = 0
        }
        touch(session, now)
        return
      }

      const chunk = frame.chunk
      if (!chunk || typeof chunk.type !== 'string') return

      switch (chunk.type) {
        case 'reasoning-delta': {
          if (typeof chunk.text === 'string' && chunk.text.trim()) {
            session.hasReasoning = true
            if (!session.approval && !session.tool) applyWorking(session)
          }
          break
        }
        case 'text-delta': {
          if (typeof chunk.text === 'string' && chunk.text.trim()) {
            session.hasText = true
            if (!session.approval && !session.tool) applyWorking(session)
          }
          break
        }
        case 'tool-call-delta': {
          if (typeof chunk.name === 'string' && chunk.name) {
            session.tool = chunk.name
            session.activity = chunk.name
            if (!session.approval) applyWorking(session)
          }
          break
        }
        case 'block-end': {
          const block = chunk.block
          if (block?.type === 'tool-call' && typeof block.name === 'string' && block.name) {
            session.tool = block.name
            session.activity = block.name
            // The complete argument JSON only lands here, never in the deltas.
            if (block.name === 'todo_write') applyTodos(session, block.arguments)
            if (!session.approval) applyWorking(session)
          }
          break
        }
        case 'usage': {
          const entry = applyMessageTokens(session, chunk.usage)
          if (entry) {
            session.attempts.set(String(frame.attemptId ?? 'latest'), entry)
            while (session.attempts.size > MAX_TRACKED_ATTEMPTS) {
              session.attempts.delete(session.attempts.keys().next().value)
            }
            foldAttempts(session)
            session.contextUsed = entry.input + entry.cacheRead
          }
          break
        }
        case 'finish': {
          const kind = chunk.reason?.kind
          if (kind === 'tool-calls') {
            if (!session.approval) applyWorking(session)
          } else if (kind === 'aborted') {
            session.status = 'aborted'
            session.indicator = 'none'
            session.statusText = '已中断'
            session.tool = ''
            session.runStartedAt = 0
          } else if (kind === 'error') {
            session.status = 'error'
            session.indicator = 'error'
            session.statusText = errorText(chunk.reason?.failure) || '会话出错'
          } else {
            session.tool = ''
            if (!session.approval) applyWorking(session)
          }
          break
        }
        default:
          break
      }
      touch(session, now)
    } catch (error) {
      warn('agent/assistant-stream failed', String(error))
    }
  })

  listen('agent/error', (payload) => {
    try {
      const id = payload?.agent?.id
      if (!id) return
      const session = sessions.get(String(id))
      if (!session) return
      session.status = 'error'
      session.indicator = 'error'
      session.statusText = errorText(payload.error) || '会话出错'
      session.tool = ''
      session.approval = ''
      session.runStartedAt = 0
      touch(session)
    } catch (error) {
      warn('agent/error failed', String(error))
    }
  })

  // Waterfall listeners must always forward exactly once, so the approval flow
  // is unchanged even if the bookkeeping above throws.
  function aroundAsk(event, kind, next) {
    const id = event?.agent?.id
    const session = id ? sessions.get(String(id)) : null
    if (session) applyWaiting(session, kind)
    let result
    try {
      result = typeof next === 'function' ? next() : undefined
    } catch (error) {
      if (session) {
        session.approval = ''
        session.waiting = ''
        if (session.status === 'waiting') applyWorking(session)
        markDirty(session)
      }
      throw error
    }
    const settle = () => {
      if (!session) return
      if (session.waiting === kind) {
        session.approval = ''
        session.waiting = ''
        if (session.status === 'waiting') applyWorking(session)
        session.updatedAt = Date.now()
        markDirty(session)
      }
    }
    if (result && typeof result.then === 'function') return result.then(
      (value) => {
        settle()
        return value
      },
      (error) => {
        settle()
        throw error
      },
    )
    settle()
    return result
  }

  listen('approval/request', function (event, next) {
    return aroundAsk(event, 'approval', next)
  })
  listen('user-questions/request', function (event, next) {
    return aroundAsk(event, 'question', next)
  })

  // ----------------------------------------------------------------- control

  const controlServer = net.createServer((socket) => {
    socket.setEncoding('utf8')
    socket.on('error', () => {})
    let buffer = ''
    socket.on('data', (chunk) => {
      buffer += chunk
      if (buffer.length > 65536) buffer = buffer.slice(-4096)
      let index = buffer.indexOf('\n')
      while (index >= 0) {
        const line = buffer.slice(0, index).trim()
        buffer = buffer.slice(index + 1)
        if (line) handleControlLine(line, socket)
        index = buffer.indexOf('\n')
      }
    })
  })
  controlServer.on('error', (error) => warn('control listener failed:', String(error?.message || error)))

  function handleControlLine(line, socket) {
    let message
    try {
      message = JSON.parse(line)
    } catch {
      return
    }
    const action = typeof message?.action === 'string' ? message.action : ''
    const sessionId = typeof message?.sessionId === 'string' ? message.sessionId : ''
    if (action === 'focus' && sessionId) {
      pendingFocus = { sessionId, at: Date.now() }
      log(`focus requested for ${sessionId}`)
      return
    }
    if (action === 'ping') {
      log('control ping')
      return
    }
    if (action === 'dump') {
      const body = JSON.stringify({
        ok: true,
        ports: { status: STATUS_PORT, control: CONTROL_PORT },
        connected,
        tracked: sessions.size,
        stats: { ...stats, events: Object.fromEntries(stats.events) },
        rows: [...sessions.values()].map((session) => ({
          id: session.id,
          hasRun: session.hasRun,
          status: session.status,
          statusText: session.statusText,
          lastActivityAt: session.lastActivityAt,
          ageMs: session.lastActivityAt ? Date.now() - session.lastActivityAt : null,
          hasGoal: Boolean(session.goal),
          todos: Array.isArray(session.todos) ? session.todos.length : 0,
          progress: goalProgress(session),
        })),
      })
      log('DUMP ' + body)
      try {
        socket?.write(`${body}\n`)
      } catch {}
    }
  }

  try {
    controlServer.listen(CONTROL_PORT, HOST)
  } catch (error) {
    warn('control listener unavailable:', String(error?.message || error))
  }

  function readPendingFocus() {
    if (!pendingFocus) return null
    if (Date.now() - pendingFocus.at > FOCUS_TTL_MS) {
      pendingFocus = null
      return null
    }
    return pendingFocus
  }

  // -------------------------------------------------------------- web routes

  ctx.inject(['webServer'], (webCtx) => {
    const webServer = webCtx.webServer
    if (!webServer?.register) return

    // `register` returns the disposer that releases the route, and a duplicate
    // (kind, path) is a hard error. Collecting the disposers into this fiber is
    // what lets the plugin be reloaded without colliding with its own ghost.
    const routeDisposers = []
    const addRoute = (route) => {
      try {
        routeDisposers.push(webServer.register(route))
      } catch (error) {
        warn(`route unavailable: ${route.path}`, String(error?.message || error))
      }
    }

    const sendJson = (res, status, body) => {
      res.writeHead(status, { 'content-type': 'application/json', 'cache-control': 'no-store' })
      res.end(JSON.stringify(body))
    }

    addRoute({
      kind: 'exact',
      path: '/api-dsh-status/pending',
      handler: (_req, res) => {
        const pending = readPendingFocus()
        sendJson(res, 200, pending
          ? { ok: true, sessionId: pending.sessionId, at: pending.at }
          : { ok: true, sessionId: '', at: 0 })
      },
    })

    addRoute({
      kind: 'exact',
      path: '/api-dsh-status/focus-done',
      handler: (req, res) => {
        let body = ''
        req.on('data', (chunk) => {
          body += chunk
          if (body.length > 65536) req.destroy()
        })
        req.on('end', () => {
          let parsed = null
          try {
            parsed = body ? JSON.parse(body) : null
          } catch {}
          if (!pendingFocus || !parsed || parsed.at === pendingFocus.at) pendingFocus = null
          sendJson(res, 200, { ok: true })
        })
        req.on('error', () => sendJson(res, 200, { ok: false }))
      },
    })

    addRoute({
      kind: 'exact',
      path: '/api-dsh-status/sessions',
      handler: (_req, res) => {
        sendJson(res, 200, {
          ok: true,
          source: SOURCE,
          instanceId: INSTANCE_ID,
          sessions: [...sessions.values()].map((session) => ({
            id: session.id,
            name: session.title,
            cwd: session.cwd,
            status: session.status,
            statusText: session.statusText,
            hasRun: session.hasRun,
            lastActivityAt: session.lastActivityAt,
          })),
        })
      },
    })

    // The Client half reads DSH's own activity wording out of the locale
    // service and reports it here, so this plugin never carries a translation
    // table and the taskbar text follows whatever language DSH is showing.
    addRoute({
      kind: 'exact',
      path: '/api-dsh-status/labels',
      handler: (req, res) => {
        let body = ''
        req.on('data', (chunk) => {
          body += chunk
          if (body.length > 262144) req.destroy()
        })
        req.on('end', () => {
          let parsed = null
          try {
            parsed = body ? JSON.parse(body) : null
          } catch {}
          const labels = parsed?.labels
          if (labels && typeof labels === 'object') {
            const next = {}
            for (const [key, value] of Object.entries(labels)) {
              if (typeof value === 'string' && value) next[key] = value
            }
            activityLabels = next
            log(`labels updated (${Object.keys(next).length} entries)`)
            // The wording changed, so every live session needs re-publishing.
            for (const session of sessions.values()) markDirty(session)
          }
          sendJson(res, 200, { ok: true })
        })
        req.on('error', () => sendJson(res, 200, { ok: false }))
      },
    })

    // Release the routes with this fiber. Without it a reload collides with its
    // own ghost: `register` throws on a duplicate (kind, path), and the throwing
    // callback is what produced "duplicate exact route" on every reload.
    const releaseRoutes = () => {
      for (const dispose of routeDisposers) {
        try {
          dispose()
        } catch {}
      }
    }
    try {
      webCtx.effect(() => releaseRoutes)
    } catch (error) {
      warn('route disposer unavailable:', String(error?.message || error))
    }
  })

  // ------------------------------------------------------------- lifecycle

  // Everything is wired, so adopt whatever is already running before the first
  // heartbeat: after a hot reload the live sessions never re-announce
  // themselves, and without this their cards would never appear.
  seedLiveSessions()

  const heartbeatTimer = setInterval(() => {
    void heartbeat().catch(() => {})
    // Self-report every 30s so the host log alone answers "is it publishing".
    if (Date.now() - lastDiagnosticAt >= 30000) {
      lastDiagnosticAt = Date.now()
      log(
        'STATS ' +
          JSON.stringify({
            tracked: sessions.size,
            running: [...sessions.values()].filter((session) => session.hasRun).length,
            connected,
            events: Object.fromEntries(stats.events),
            gatedNoRun: stats.gatedNoRun,
            gatedSame: stats.gatedSame,
            sent: stats.sent,
            sendOk: stats.sendOk,
            sendFail: stats.sendFail,
          }),
      )
    }
  }, HEARTBEAT_MS)
  heartbeatTimer.unref?.()

  const cleanup = () => {
    if (disposed) return
    disposed = true
    clearInterval(heartbeatTimer)
    if (publishTimer) clearTimeout(publishTimer)
    try {
      controlServer.close()
    } catch {}
    const ids = [...sessions.keys()]
    sessions.clear()
    for (const id of ids) void publishRemove(id).catch(() => {})
  }

  try {
    ctx.effect(() => cleanup)
  } catch {
    listen('dispose', cleanup)
  }

  log(`publishing to ${HOST}:${STATUS_PORT}, control on ${CONTROL_PORT}`)
}
