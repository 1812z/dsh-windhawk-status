/**
 * Loads the Client half the way the Harness Web UI does — through
 * window.__ModuleLoader__ — then feeds it one parked focus request and checks
 * that it selects the conversation and acknowledges the request.
 *
 *   node tools/check-client.mjs
 */

const sessionId = 'session-8e65b0cf-47d2-4f24-bbc1-bceb0c2ad63e'
const at = 1791040819985

let loaded = null
const requests = []
const bodies = new Map()

globalThis.window = {
  __ModuleLoader__: {
    load(definition) {
      loaded = definition
    },
  },
  focus() {
    requests.push('window.focus')
  },
}

globalThis.fetch = async (url, init = {}) => {
  requests.push(`${init.method ?? 'GET'} ${url}`)
  if (init.body) {
    try {
      bodies.set(url, JSON.parse(init.body))
    } catch {}
  }
  if (url === '/api-dsh-status/pending') {
    return { ok: true, status: 200, json: async () => ({ ok: true, sessionId, at }) }
  }
  return { ok: true, status: 200, json: async () => ({ ok: true }) }
}

await import('../client.js')

if (!loaded) throw new Error('client.js did not call __ModuleLoader__.load')
if (loaded.id !== 'dsh-windhawk-status') throw new Error(`unexpected module id: ${loaded.id}`)

// DSH's locale service, shaped like the real one: `bind(ns)` returns a
// translate function that echoes the key back for anything unregistered.
const dictionaries = {
  'message.stepProcess.commands': '正在运行命令',
  'message.stepProcess.edit': '正在编辑文件',
  'message.turnProcess.worked': '已完成',
}
const locale = {
  getSnapshot: () => ({ active: 'zh-CN', locales: [], revision: 7 }),
  bind: () => (key) => dictionaries[key] ?? key,
}

const opened = []
const disposers = []
const module = loaded.factory()
module.apply({
  get: (name) => {
    if (name === 'uiWorkspace') return { openSession: (id) => opened.push(id) }
    if (name === 'locale') return locale
    return undefined
  },
  effect: (callback) => disposers.push(callback()),
})

// The first poll runs immediately; give the microtask queue room.
await new Promise((resolve) => setTimeout(resolve, 60))

for (const disposer of disposers) disposer()

const labels = bodies.get('/api-dsh-status/labels')?.labels
const problems = []
if (opened.length !== 1 || opened[0] !== sessionId) {
  problems.push('openSession was not called with the parked session')
}
if (!labels) {
  problems.push('DSH labels were never reported to the Host')
} else {
  if (labels['stepProcess.commands'] !== '正在运行命令') {
    problems.push(`stepProcess.commands label wrong: ${labels['stepProcess.commands']}`)
  }
  // An unregistered key must be dropped rather than shipped as the key itself.
  for (const [key, value] of Object.entries(labels)) {
    if (key === value) problems.push(`key echoed back as its own label: ${key}`)
  }
}

console.log('module id     :', loaded.id)
console.log('module name   :', module.name)
console.log('openSession   :', JSON.stringify(opened))
console.log('requests      :', requests.join(' | '))
console.log('acknowledged  :', requests.some((entry) => entry.startsWith('POST /api-dsh-status/focus-done')))
console.log('labels sent   :', labels ? `${Object.keys(labels).length} (${Object.keys(labels).join(', ')})` : 'NONE')
console.log(problems.length ? `\nFAIL: ${problems.join('; ')}` : '\nPASS')
process.exit(problems.length ? 1 : 0)
