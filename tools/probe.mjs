/**
 * Talk to the running plugin's control port.
 *
 *   node tools/probe.mjs                 # dump the plugin's live state
 *   node tools/probe.mjs ping            # prove the channel works
 *   node tools/probe.mjs focus <session> # ask it to open a conversation
 *
 * The control port is plain NDJSON on 127.0.0.1:19289 and needs no auth, which
 * makes this the fastest way to see what the plugin actually thinks.
 */
import net from 'node:net'

const argv = process.argv.slice(2)
const flag = (name, fallback) => {
  const index = argv.indexOf(`--${name}`)
  return index >= 0 && argv[index + 1] ? argv[index + 1] : fallback
}

const PORT = Number(flag('port', process.env.DSH_STATUS_CONTROL_PORT ?? 19289))
const HOST = flag('host', process.env.DSH_STATUS_HOST ?? '127.0.0.1')
const action = argv[0] && !argv[0].startsWith('--') ? argv[0] : 'dump'
const sessionId = argv[1] ?? ''

const message =
  action === 'focus' ? { action: 'focus', sessionId } : { action }

const socket = net.createConnection({ host: HOST, port: PORT })
let reply = ''

socket.setEncoding('utf8')
socket.setTimeout(2500)

socket.on('connect', () => {
  socket.write(`${JSON.stringify(message)}\n`)
  // `dump` answers on the same socket; the other actions only log, so give the
  // write a moment to land before we stop waiting.
  if (action !== 'dump') setTimeout(() => socket.end(), 250)
})

socket.on('data', (chunk) => {
  reply += chunk
  if (reply.includes('\n')) socket.end()
})

socket.on('timeout', () => {
  console.log(`no reply on ${HOST}:${PORT} after 2.5s`)
  console.log('the plugin may not be listening, or an older instance owns the port.')
  socket.destroy()
  process.exitCode = 1
})

socket.on('error', (error) => {
  console.log(`control port ${HOST}:${PORT} unreachable: ${error.message}`)
  process.exitCode = 1
})

socket.on('close', () => {
  if (reply.trim()) {
    try {
      console.log(JSON.stringify(JSON.parse(reply.trim()), null, 2))
    } catch {
      console.log(reply.trim())
    }
  } else if (action !== 'dump') {
    console.log(`sent ${JSON.stringify(message)} to ${HOST}:${PORT} (no reply expected)`)
    console.log('check the DSH host log for the plugin line.')
  }
})
