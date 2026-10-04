/**
 * Fake session producer. Lets you watch the Windhawk floating windows without
 * running DSH or OpenCode at all, and lets you exercise the right-click
 * "open session" round trip.
 *
 *   node tools/mock-producer.mjs
 *   node tools/mock-producer.mjs --count 4 --scripted
 *
 * Default ports match the DSH plugin: status 19290, control 19289. Override
 * with --port / --control or DSH_STATUS_PORT / DSH_STATUS_CONTROL_PORT.
 *
 *   --count N      how many fake sessions (default 2)
 *   --scripted     run a fixed script and exit: working -> tool -> idle ->
 *                  working -> error, so you can watch appear/auto-hide/return
 *   --plain        stay in a steady "working" state forever
 *
 * Every session sends the same newline-delimited JSON the real plugin sends,
 * so anything you see here is the mod, not the mock.
 */

import net from 'node:net'

const argv = process.argv.slice(2)
const flag = (name, fallback) => {
  const index = argv.indexOf(`--${name}`)
  return index === -1 ? fallback : argv[index + 1]
}

const PORT = Number(flag('port', process.env.DSH_STATUS_PORT ?? 19290))
const CONTROL_PORT = Number(flag('control', process.env.DSH_STATUS_CONTROL_PORT ?? 19289))
const COUNT = Number(flag('count', 2))
const SCRIPTED = argv.includes('--scripted')
const PLAIN = argv.includes('--plain')
const HEARTBEAT_MS = 5000

const now = () => Date.now()

const SEEDS = [
  {
    name: 'dsh-windhawk-status',
    directory: 'C:\\Users\\1812z\\Documents\\GitHub\\dsh-windhawk-status',
    provider: 'zhuhjay',
    model: 'global:deepseek-v4.1-flash',
  },
  {
    name: 'opencode-windhawk-status',
    directory: 'C:\\Users\\1812z\\Documents\\GitHub\\OpenCode_Windhawk_Status_Plugin',
    provider: 'intern',
    model: 'DeepSeek-V4-Pro-0813',
  },
  {
    name: 'dsh-chat-import',
    directory: 'C:\\Users\\1812z\\Documents\\GitHub\\dsh-chat-import',
    provider: 'intern',
    model: 'GLM-5.3',
  },
  {
    name: 'blog-draft',
    directory: 'C:\\Users\\1812z\\Documents\\Writing',
    provider: 'zhuhjay',
    model: 'global:deepseek-v4.1-flash',
  },
]

/** The states a scripted session walks through. */
const SCRIPT = [
  {
    seconds: 6,
    patch: { status: 'working', statusText: '等待中', indicator: 'working', activity: '' },
  },
  {
    seconds: 6,
    patch: { status: 'working', statusText: '思考中', indicator: 'working', activity: '' },
  },
  {
    seconds: 8,
    patch: { status: 'working', statusText: 'Bash', indicator: 'working', activity: 'Bash' },
  },
  {
    seconds: 8,
    patch: {
      status: 'working',
      statusText: 'Edit',
      indicator: 'working',
      activity: 'Edit',
      progress: { current: 3, completed: 2, total: 5, label: '第 2/5 步', task: 'TodoWrite' },
    },
  },
  {
    seconds: 5,
    patch: {
      status: 'waiting',
      statusText: '等待审批',
      indicator: 'approval',
      approval: 'approval',
      activity: '',
    },
  },
  {
    seconds: 4,
    patch: { status: 'idle', statusText: '已完成', indicator: 'none', approval: '', activity: '' },
  },
]

function makeSession(index) {
  const seed = SEEDS[index % SEEDS.length]
  const id = `session-mock-${index}-${Math.random().toString(16).slice(2, 10)}`
  return {
    instanceId: `${process.env.COMPUTERNAME ?? 'host'}:dsh:${process.pid}:session:${id}`,
    session: {
      id,
      name: seed.name,
      directory: seed.directory,
      status: 'idle',
      statusText: '等待输入',
      activity: '',
      indicator: 'none',
      approval: '',
      waitingForOutput: false,
      hasRun: false,
      runStartedAt: 0,
      lastActivityAt: now(),
      progress: { current: 0, completed: 0, total: 0, label: '', task: '' },
      model: { providerId: seed.provider, modelId: seed.model },
      tokens: {
        input: 0,
        output: 0,
        reasoning: 0,
        cacheRead: 0,
        cacheWrite: 0,
        total: 0,
        contextUsed: 0,
        contextLimit: 200000,
        contextPercent: 0,
      },
    },
  }
}

const sessions = Array.from({ length: Math.max(1, COUNT) }, (_, index) => makeSession(index))

function refreshTokens(record, drift) {
  const tokens = record.session.tokens
  tokens.input += 900 + Math.round(Math.random() * 2600)
  tokens.output += 60 + Math.round(Math.random() * 260)
  tokens.reasoning += 30 + Math.round(Math.random() * 180)
  tokens.cacheRead += 400 + Math.round(Math.random() * 1200)
  tokens.total = tokens.input + tokens.output + tokens.reasoning
  tokens.contextUsed = tokens.input + tokens.cacheRead
  tokens.contextPercent = (tokens.contextUsed / tokens.contextLimit) * 100
  void drift
}

function applyPatch(record, patch, { startRun = false } = {}) {
  Object.assign(record.session, patch)
  if (patch.progress) record.session.progress = patch.progress
  if (startRun || patch.status === 'working') {
    record.session.hasRun = true
    if (!record.session.runStartedAt) record.session.runStartedAt = now()
  }
  if (patch.status === 'idle') record.session.runStartedAt = 0
  record.session.lastActivityAt = now()
}

function line(record, type = 'status') {
  return `${JSON.stringify({
    protocol: 'opencode-status',
    version: 2,
    type,
    source: 'dsh-status',
    app: 'DSH',
    openable: true,
    instanceId: record.instanceId,
    pid: process.pid,
    host: process.env.COMPUTERNAME ?? 'host',
    timestamp: now(),
    session: record.session,
  })}\n`
}

function send(payload) {
  return new Promise((resolve) => {
    const socket = net.createConnection({ host: '127.0.0.1', port: PORT }, () => {
      socket.end(payload)
    })
    socket.on('close', resolve)
    socket.on('error', () => resolve())
  })
}

async function push(record, type) {
  await send(line(record, type))
}

function summarize(record) {
  const session = record.session
  return (
    `${session.name.padEnd(30).slice(0, 30)} ` +
    `${session.status.padEnd(7)} ` +
    `${session.statusText.padEnd(8)} ` +
    `${session.indicator.padEnd(8)} ` +
    `ctx=${Math.round(session.tokens.contextPercent)}%`
  )
}

// ------------------------------------------------------------- control port

const control = net.createServer((socket) => {
  let buffer = ''
  socket.setEncoding('utf8')
  socket.on('data', (chunk) => {
    buffer += chunk
  })
  socket.on('end', () => {
    for (const raw of buffer.split('\n')) {
      if (!raw.trim()) continue
      let request
      try {
        request = JSON.parse(raw)
      } catch {
        continue
      }
      if (request.action === 'focus') {
        const match = sessions.find((record) => record.session.id === request.sessionId)
        console.log(
          `\n  >> focus request: ${match ? match.session.name : request.sessionId}` +
            `\n     (the real DSH plugin would now select that conversation)\n`,
        )
      }
    }
  })
})

let controlAvailable = false
try {
  await new Promise((resolve, reject) => {
    control.once('error', reject)
    control.listen(CONTROL_PORT, '127.0.0.1', resolve)
  })
  controlAvailable = true
} catch (error) {
  // A live DSH plugin usually owns the control port. Feeding the status port is
  // the whole point of this tool, so a busy control port must not abort the run.
  control.on('error', () => {})
  console.log(`control port ${CONTROL_PORT} busy (${error.code}); no focus listener.`)
  console.log('pass --control <free port> to watch right-click requests here.')
}

// ------------------------------------------------------------------- driver

console.log(`status port  ${PORT}`)
console.log(
  controlAvailable
    ? `control port ${CONTROL_PORT}  (right-click a card to test it)`
    : `control port ${CONTROL_PORT}  (busy: the live plugin owns it)`,
)
console.log(`sessions     ${sessions.length}${SCRIPTED ? '  (scripted)' : ''}`)
console.log()

// Kick each session into its first run, staggered so the cards stack visibly.
for (const record of sessions) {
  refreshTokens(record)
  applyPatch(record, { status: 'working', statusText: '等待中', indicator: 'working' }, {
    startRun: true,
  })
  await push(record)
  console.log(`  ${summarize(record)}`)
  await new Promise((resolve) => setTimeout(resolve, 250))
}

const heartbeat = setInterval(() => {
  for (const record of sessions) void push(record, 'heartbeat')
}, HEARTBEAT_MS)

if (SCRIPTED) {
  let step = 0
  for (const stage of SCRIPT) {
    await new Promise((resolve) => setTimeout(resolve, stage.seconds * 1000))
    for (const record of sessions) {
      if (stage.patch.status === 'working') refreshTokens(record)
      applyPatch(record, stage.patch)
      await push(record)
    }
    step++
    console.log(`  stage ${step}/${SCRIPT.length}  ${summarize(sessions[0])}`)
  }
  console.log()
  console.log('script done. cards should stay for the idle timeout, then vanish.')
  console.log(`the mod's default is 10 minutes, so they will linger. keep this`)
  console.log('process alive to keep the sessions alive, or Ctrl+C to drop them now.')
} else if (PLAIN) {
  console.log()
  console.log('steady working state. Ctrl+C to stop.')
} else {
  // Drift: keep every session alive and occasionally rotate a tool name, so
  // the cards animate instead of sitting frozen.
  const TOOLS = ['Bash', 'Read', 'Grep', 'Edit', 'Glob', 'WebSearch', 'Task']
  const drift = setInterval(() => {
    for (const record of sessions) {
      refreshTokens(record)
      const tool = TOOLS[Math.floor(Math.random() * TOOLS.length)]
      record.session.status = 'working'
      record.session.indicator = 'working'
      record.session.statusText = tool
      record.session.activity = tool
      record.session.lastActivityAt = now()
      void push(record)
    }
    console.log(`  ${summarize(sessions[0])}`)
  }, 3000)
  drift.unref?.()
}

const shutdown = () => {
  console.log('\nstopping: dropping every session')
  clearInterval(heartbeat)
  if (controlAvailable) control.close()
  for (const record of sessions) void send(line(record, 'remove'))
  setTimeout(() => process.exit(0), 300)
}

process.on('SIGINT', shutdown)
process.on('SIGTERM', shutdown)
