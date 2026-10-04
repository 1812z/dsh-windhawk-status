/**
 * Boots the Host half against a fake Cordis context, drives it through a
 * realistic session lifecycle, and prints every message it publishes. Also
 * exercises the control port and the two web routes.
 *
 *   node tools/check-plugin.mjs
 *
 * Nothing here touches a real profile: no DSH services are needed, because the
 * plugin treats all of them as optional.
 *
 * Ports default to 19390/19391 rather than the shipped 19290/19289 on purpose.
 * A live DSH already owns the control port, and talking to it instead of to the
 * instance under test is exactly the kind of thing this script is meant to
 * catch. Override with DSH_STATUS_PORT / DSH_STATUS_CONTROL_PORT.
 */

import net from 'node:net'

const STATUS_PORT = Number(process.env.DSH_STATUS_PORT ?? 19390)
const CONTROL_PORT = Number(process.env.DSH_STATUS_CONTROL_PORT ?? 19391)

// index.js reads these once, at import time, so pin them before importing it.
process.env.DSH_STATUS_PORT = String(STATUS_PORT)
process.env.DSH_STATUS_CONTROL_PORT = String(CONTROL_PORT)

const received = []
const routes = new Map()
const scopeDisposers = []
const handlers = new Map()
let disposer = null

const PREEXISTING_ID = 'session-preexisting-0000'
const CHILD_ID = 'session-child-0000'
const FINISHED_ID = 'session-finished-0000'

// Session logs: PREEXISTING_ID has an open turn (mid-run at load time) and a
// task list, FINISHED_ID closed its last turn.
const TODO_ARGUMENTS = JSON.stringify({
  todos: [
    { content: '读取 DSH 工具列表', status: 'completed' },
    { content: '把文案接到任务栏', status: 'in_progress' },
    { content: '编译验证', status: 'pending' },
  ],
})
const logs = {
  [PREEXISTING_ID]: {
    events: [
      { type: 'turn/start' },
      { type: 'step/start' },
      { type: 'tool/call', data: { name: 'todo_write', arguments: TODO_ARGUMENTS } },
    ],
  },
  [FINISHED_ID]: {
    events: [{ type: 'turn/start' }, { type: 'turn/end' }],
  },
}

// The live agent registry. `goals.get()` must receive one of these exact
// objects: the real service validates identity and throws for a lookalike,
// which is why a synthetic `{ id }` argument silently produced no progress.
const liveAgents = new Map([
  [PREEXISTING_ID, { id: PREEXISTING_ID }],
  [FINISHED_ID, { id: FINISHED_ID }],
])
const goalViews = new Map([
  [PREEXISTING_ID, { maxGoalRounds: 5, roundsStarted: 2, objective: 'ship the widget' }],
])

// ---------------------------------------------------------------- fake ctx

const fakeCtx = {
  logger: { info: () => {}, warn: () => {} },
  get(name) {
    // A session store that already holds two live sessions before the plugin
    // loads: one ordinary, one subagent child. Only the first should be adopted.
    if (name === 'sessions') {
      return {
        list: () => [
          { id: PREEXISTING_ID, header: { cwd: 'C:\\preexisting' } },
          { id: CHILD_ID, header: { cwd: 'C:\\child', origin: 'subagent' } },
          { id: FINISHED_ID, header: { cwd: 'C:\\finished' } },
        ],
      }
    }
    if (name === 'sessionQuery') {
      return {
        readSession: async (id) => {
          if (!logs[id]) throw new Error('no log')
          return logs[id]
        },
      }
    }
    if (name === 'agents') {
      return {
        get: (id) => liveAgents.get(id),
        list: () => [...liveAgents.values()],
      }
    }
    if (name === 'goals') {
      return {
        get(agent) {
          if (![...liveAgents.values()].includes(agent)) {
            throw new Error('GoalError: not the registry live instance')
          }
          return goalViews.get(agent.id)
        },
      }
    }
    return undefined
  },
  on(name, handler) {
    if (!handlers.has(name)) handlers.set(name, [])
    handlers.get(name).push(handler)
  },
  inject(names, callback) {
    // A real inject callback gets a scoped ctx: same services, plus `effect`.
    const scope = {
      webServer: {
        register(route) {
          if (routes.has(route.path)) {
            throw new Error(`webserver: duplicate exact route "${route.path}"`)
          }
          routes.set(route.path, route)
          return () => routes.delete(route.path)
        },
      },
      effect(callback) {
        scopeDisposers.push(callback())
      },
    }
    callback(scope)
  },
  effect(callback) {
    disposer = callback()
  },
}

function emit(name, ...args) {
  for (const handler of handlers.get(name) ?? []) handler(...args)
}

// ------------------------------------------------------------- fake browser

function callRoute(path, { method = 'GET', body = '' } = {}) {
  return new Promise((resolve) => {
    const route = routes.get(path)
    if (!route) {
      resolve({ status: 404, body: null })
      return
    }
    const listeners = { data: [], end: [], error: [] }
    const req = {
      method,
      on(event, fn) {
        listeners[event]?.push(fn)
        return req
      },
      destroy() {},
    }
    const res = {
      status: 0,
      writeHead(status) {
        this.status = status
      },
      end(text) {
        resolve({ status: this.status, body: text ? JSON.parse(text) : null })
      },
    }
    Promise.resolve(route.handler(req, res)).then(() => {
      if (body) for (const fn of listeners.data) fn(body)
      for (const fn of listeners.end) fn()
    })
  })
}

function sendControl(payload) {
  return new Promise((resolve) => {
    const socket = net.createConnection({ host: '127.0.0.1', port: CONTROL_PORT }, () => {
      socket.end(`${JSON.stringify(payload)}\n`)
    })
    socket.on('close', resolve)
    socket.on('error', resolve)
  })
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

// ---------------------------------------------------------------- listener

const server = net.createServer((socket) => {
  let buffer = ''
  socket.setEncoding('utf8')
  socket.on('data', (chunk) => {
    buffer += chunk
  })
  socket.on('end', () => {
    for (const line of buffer.split('\n')) {
      if (line.trim()) received.push(JSON.parse(line))
    }
  })
})

await new Promise((resolve) => server.listen(STATUS_PORT, '127.0.0.1', resolve))

const { apply } = await import('../index.js')
apply(fakeCtx, {})

// ------------------------------------------------------------------- adoption

// The store was already populated before apply(). Adoption publishes a session
// that is mid-turn (its running state never toggles, so nothing else would ever
// reveal it) and stays silent for one whose last turn already closed.
await sleep(200)
const adoptionProblems = []
const publishedIds = () =>
  new Set(received.filter((message) => message.session).map((message) => message.session.id))

const adoptedNow = received.filter((message) => message.session?.id === PREEXISTING_ID)
if (!adoptedNow.length) {
  adoptionProblems.push('a session that was already mid-turn was not published')
} else {
  const latest = adoptedNow.at(-1)
  if (latest.session.hasRun !== true) {
    adoptionProblems.push('an adopted mid-turn session must report hasRun')
  }
  if (latest.session.directory !== 'C:\\preexisting') {
    adoptionProblems.push(`adopted cwd not carried over: ${latest.session.directory}`)
  }
  if (latest.session.status !== 'working') {
    adoptionProblems.push(`adopted mid-turn session must be working: ${latest.session.status}`)
  }
  // {progress}/{task} come from the todo list, recovered from the session log at
  // adoption. PREEXISTING_ID also has a goal, so this asserts the todo list wins
  // over the goal round counter.
  if (latest.session.progress?.label !== '1/3') {
    adoptionProblems.push(`progress label must be 1/3, got ${JSON.stringify(latest.session.progress)}`)
  }
  if (latest.session.progress?.task !== '把文案接到任务栏') {
    adoptionProblems.push(
      `progress task must be the in-progress item, got ${JSON.stringify(latest.session.progress)}`,
    )
  }
}
if (publishedIds().has(FINISHED_ID)) {
  adoptionProblems.push('a session whose turn already ended must not be published')
}
if (publishedIds().has(CHILD_ID)) {
  adoptionProblems.push('subagent child must never be published')
}

// ------------------------------------------------------- self-healing events

// A session that was never announced must still be tracked: this is what a
// hot reload looks like when a turn is already in flight.
const UNANNOUNCED_ID = 'session-unannounced-0000'
emit('agent/status', { agent: { id: UNANNOUNCED_ID }, status: 'running' })
await sleep(150)
const healed = received.find((message) => message.session?.id === UNANNOUNCED_ID)
if (!healed) {
  adoptionProblems.push('agent/status for an unannounced session produced no message')
} else if (healed.session.hasRun !== true || healed.session.status !== 'working') {
  adoptionProblems.push(`self-healed session has the wrong state: ${JSON.stringify(healed.session)}`)
}

// --------------------------------------------------------- wire contract

// The mod reads these off the root object and nothing else. `openable` in
// particular decides whether a click jumps to a conversation or opens a
// terminal, so a nested copy would be silently ignored in production.
const contractProblems = []
const sample = received.find((message) => message.session)
if (!sample) {
  contractProblems.push('no session message captured')
} else {
  if (sample.protocol !== 'opencode-status') contractProblems.push('protocol mismatch')
  if (sample.version !== 2) contractProblems.push('version mismatch')
  if (sample.source !== 'dsh-status') contractProblems.push('source missing')
  if (sample.app !== 'DSH') contractProblems.push('app missing (mod renders {app})')
  if (sample.openable !== true) contractProblems.push('openable must be a root boolean')
  if (sample.session && 'openable' in sample.session) {
    contractProblems.push('openable must not be nested under session')
  }
  if (typeof sample.instanceId !== 'string' || !sample.instanceId) {
    contractProblems.push('instanceId missing')
  }
  if (!Number.isFinite(sample.pid)) contractProblems.push('pid missing')
  for (const field of ['name', 'status', 'statusText', 'lastActivityAt', 'runStartedAt']) {
    if (!(field in sample.session)) contractProblems.push(`session.${field} missing`)
  }
  if (typeof sample.session.lastActivityAt !== 'number' || sample.session.lastActivityAt <= 0) {
    contractProblems.push('session.lastActivityAt must be epoch ms (mod uses it for idle timeout)')
  }
  if (!sample.session.model || !sample.session.tokens || !sample.session.progress) {
    contractProblems.push('session.model/tokens/progress missing')
  }
}

// --------------------------------------------------------- DSH-owned wording

// The Client half reports DSH's own activity wording; the Host must classify the
// tool id and attach that exact text, keeping the id for matching. Nothing in
// the plugin may invent a translation, so the text must equal what was posted.
const labelProblems = []
const labelsResponse = await callRoute('/api-dsh-status/labels', {
  method: 'POST',
  body: JSON.stringify({
    labels: {
      'stepProcess.commands': '正在运行命令',
      'turnProcess.worked': '已完成',
    },
  }),
})
if (labelsResponse.status !== 200) {
  labelProblems.push(`labels route returned ${labelsResponse.status}`)
}
await sleep(150)

async function sendTool(name) {
  const before = received.length
  emit('agent/assistant-stream', {
    agent: { id: UNANNOUNCED_ID },
    frame: { type: 'chunk', chunk: { type: 'tool-call-delta', name } },
  })
  await sleep(160)
  return received.slice(before).filter((message) => message.session?.activityId === name).at(-1)
}

const knownTool = await sendTool('pwsh')
if (!knownTool) {
  labelProblems.push('a tool-call-delta for pwsh produced no message')
} else {
  if (knownTool.session.activity !== '正在运行命令') {
    labelProblems.push(`activity must be DSH's wording, got "${knownTool.session.activity}"`)
  }
  if (knownTool.session.statusText !== '正在运行命令') {
    labelProblems.push(`statusText must follow the tool, got "${knownTool.session.statusText}"`)
  }
  if (knownTool.session.statusId !== 'commands') {
    labelProblems.push(`pwsh must classify as commands, got "${knownTool.session.statusId}"`)
  }
}

// An unmapped tool keeps its id as the display text, so nothing ever goes blank.
const unknownTool = await sendTool('mcp__thing__do_it')
if (!unknownTool) {
  labelProblems.push('an unmapped tool produced no message')
} else {
  if (unknownTool.session.activity !== 'mcp__thing__do_it') {
    labelProblems.push(`unmapped tool must fall back to its id, got "${unknownTool.session.activity}"`)
  }
  if (unknownTool.session.statusId !== 'tools') {
    labelProblems.push(`unmapped tool must classify as tools, got "${unknownTool.session.statusId}"`)
  }
}

// ----------------------------------------------------------------- task list

// The live path. `todo_write` carries the COMPLETE list, and its argument JSON
// only lands on block-end — the deltas never carry it.
const taskProblems = []
async function sendTodoWrite(argumentsText) {
  const before = received.length
  emit('agent/assistant-stream', {
    agent: { id: UNANNOUNCED_ID },
    frame: {
      type: 'chunk',
      chunk: {
        type: 'block-end',
        block: { type: 'tool-call', id: 'call-todo', name: 'todo_write', arguments: argumentsText },
      },
    },
  })
  await sleep(170)
  return received.slice(before).filter((message) => message.session?.id === UNANNOUNCED_ID).at(-1)
}

const todoMessage = await sendTodoWrite(
  JSON.stringify({
    todos: [
      { content: '一', status: 'completed' },
      { content: '二', status: 'completed' },
      { content: '三', status: 'in_progress' },
      { content: '四', status: 'pending' },
    ],
  }),
)
if (!todoMessage) {
  taskProblems.push('a todo_write block produced no message')
} else {
  if (todoMessage.session.progress?.label !== '2/4') {
    taskProblems.push(`live progress must be 2/4, got ${JSON.stringify(todoMessage.session.progress)}`)
  }
  if (todoMessage.session.progress?.task !== '三') {
    taskProblems.push(`live task must be the in-progress item, got ${JSON.stringify(todoMessage.session.progress)}`)
  }
}

// A malformed payload must be ignored rather than clearing a list already shown.
const afterMalformed = await sendTodoWrite('{ not json')
const survived = afterMalformed ?? todoMessage
if (survived?.session.progress?.label !== '2/4') {
  taskProblems.push('a malformed todo payload must not clear the task list')
}

// ------------------------------------------------------------- the lifecycle

const sessionId = 'session-8e65b0cf-47d2-4f24-bbc1-bceb0c2ad63e'

emit('api-session/added', {
  agentAvailable: true,
  sessionId,
  updatedAt: Date.now(),
  running: false,
  blank: false,
  cwd: 'C:\\Users\\1812z\\Documents\\GitHub\\dsh-windhawk-status',
})
await sleep(120)

emit('api-session/status', sessionId, true)
emit('agent/status', { agent: { id: sessionId }, status: 'running' })
await sleep(120)

emit('agent/assistant-stream', {
  agent: { id: sessionId },
  frame: { type: 'start', attemptId: 'attempt-1', revision: 1, turn: 1, step: 1 },
})
emit('agent/assistant-stream', {
  agent: { id: sessionId },
  frame: {
    type: 'chunk',
    attemptId: 'attempt-1',
    revision: 1,
    index: 0,
    time: Date.now(),
    chunk: { type: 'reasoning-delta', index: 0, text: 'thinking about the layout' },
  },
})
await sleep(120)

emit('agent/assistant-stream', {
  agent: { id: sessionId },
  frame: {
    type: 'chunk',
    attemptId: 'attempt-1',
    revision: 1,
    index: 1,
    time: Date.now(),
    chunk: { type: 'tool-call-delta', index: 1, id: 'call-1', name: 'Bash', argumentsDelta: '{}' },
  },
})
emit('agent/assistant-stream', {
  agent: { id: sessionId },
  frame: {
    type: 'chunk',
    attemptId: 'attempt-1',
    revision: 1,
    index: 2,
    time: Date.now(),
    chunk: {
      type: 'usage',
      usage: {
        inputTokens: 41200,
        outputTokens: 3180,
        totalTokens: 45230,
        cacheReadTokens: 12000,
        cacheWriteTokens: 640,
        reasoningTokens: 810,
      },
    },
  },
})
await sleep(150)

// Waiting on an approval: the waterfall handler must forward to `next`.
const approvalHandler = (handlers.get('approval/request') ?? [])[0]
let forwarded = false
if (approvalHandler) {
  await approvalHandler(
    { agent: { id: sessionId }, toolName: 'Bash' },
    async () => {
      forwarded = true
      await sleep(60)
      return 'allowed-once'
    },
  )
}
await sleep(150)

emit('api-session/status', sessionId, false)
emit('agent/status', { agent: { id: sessionId }, status: 'idle' })
await sleep(150)

// Control port -> pending focus -> the Client half's two routes.
await sendControl({ action: 'focus', sessionId })

// The control listener is a separate socket from this process, so poll instead
// of sleeping a fixed amount and hoping.
let pending = { status: 0, body: { sessionId: '', at: 0 } }
for (let attempt = 0; attempt < 40; attempt++) {
  pending = await callRoute('/api-dsh-status/pending')
  if (pending.body?.sessionId) break
  await sleep(25)
}
if (!pending.body?.sessionId) {
  console.log('WARNING: the focus request never reached the pending slot\n')
}
const done = await callRoute('/api-dsh-status/focus-done', {
  method: 'POST',
  body: JSON.stringify({ sessionId: pending.body.sessionId, at: pending.body.at }),
})
const pendingAfter = await callRoute('/api-dsh-status/pending')
const listing = await callRoute('/api-dsh-status/sessions')

await sleep(200)
disposer?.()
// Cordis disposes the plugin fiber and every inject-scope effect together.
for (const release of scopeDisposers) release?.()
await sleep(150)
server.close()

// A route left behind makes the next load throw "duplicate exact route" and the
// reload silently keeps the previous code, so released routes are a real check.
const leakedRoutes = routes.size

// ------------------------------------------------------------------ report

const statuses = received.filter((message) => message.session)
console.log('routes      :', [...routes.keys()].join(', '))
console.log('routes after dispose:', leakedRoutes === 0 ? 'released' : `${leakedRoutes} LEAKED`)
console.log('approval    :', forwarded ? 'forwarded to next()' : 'NOT FORWARDED')
console.log('adoption    :', adoptionProblems.length ? adoptionProblems.join('; ') : 'ok')
console.log('contract    :', contractProblems.length ? contractProblems.join('; ') : 'ok')
console.log('wording     :', labelProblems.length ? labelProblems.join('; ') : 'ok')
console.log('task list   :', taskProblems.length ? taskProblems.join('; ') : 'ok')
console.log('focus       :', JSON.stringify(pending.body), '->', JSON.stringify(done.body))
console.log('after done  :', JSON.stringify(pendingAfter.body))
console.log('sessions    :', JSON.stringify(listing.body?.sessions))
console.log('messages    :', received.length)
console.log('instance id :', statuses[0]?.instanceId)
console.log()

const interesting = []
const seen = new Set()
for (const message of statuses) {
  const key = `${message.type}|${message.session.status}|${message.session.statusText}|${message.session.activity}|${message.session.indicator}|${message.session.tokens.total}`
  if (seen.has(key)) continue
  seen.add(key)
  interesting.push(message)
}
for (const message of interesting) {
  const session = message.session
  console.log(
    `${message.type.padEnd(9)} ${session.status.padEnd(7)} ` +
      `status_text=${JSON.stringify(session.statusText).padEnd(12)} ` +
      `activity=${JSON.stringify(session.activity).padEnd(8)} ` +
      `indicator=${session.indicator.padEnd(8)} ` +
      `hasRun=${session.hasRun ? 1 : 0} ` +
      `tok=${session.tokens.total} ctx=${session.tokens.contextUsed}/${session.tokens.contextLimit}`,
  )
}

const removals = received.filter((message) => message.type === 'remove')
console.log()
console.log('remove messages after dispose:', removals.length)
console.log('sample payload:')
console.log(JSON.stringify(interesting.at(-1), null, 2))

if (
  adoptionProblems.length ||
  contractProblems.length ||
  labelProblems.length ||
  taskProblems.length ||
  leakedRoutes
) {
  console.log(
    '\nFAIL: ' +
      [
        ...adoptionProblems,
        ...contractProblems,
        ...labelProblems,
        ...taskProblems,
        leakedRoutes ? 'web routes leaked' : '',
      ]
        .filter(Boolean)
        .join('; '),
  )
  process.exit(1)
}
