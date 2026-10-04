/**
 * dsh-windhawk-status — Client half.
 *
 * The Windhawk mod can raise the Desktop window, but it cannot select a
 * conversation inside the page. So the Host half parks a focus request and this
 * module drains it: poll the route, open the session, report back.
 *
 * A request is identified by the Host-issued `at` stamp rather than by the
 * session id, so asking for the same session twice in a row still navigates.
 */
window.__ModuleLoader__.load({
  id: 'dsh-windhawk-status',
  factory() {
    const POLL_MS = 600

    /** DSH's activity categories, as declared by the chat UI's own dictionary. */
    const ACTIVITY_CATEGORIES = [
      'thinking', 'read', 'readImage', 'write', 'search', 'edit',
      'commands', 'code', 'webSearch', 'webFetch', 'subagents', 'plan',
      'questions', 'tools',
    ]

    return {
      name: 'dsh-windhawk-status/client',
      apply(ctx) {
        let disposed = false
        let busy = false
        let lastAt = 0
        let sentRevision = null

        /**
         * Report DSH's own wording for each activity category to the Host.
         *
         * The Host never carries a translation table: it classifies a tool id
         * into one of these categories and prints whatever text arrives here,
         * so the taskbar follows the language DSH is already using. `t()` echoes
         * the key back when a dictionary has not registered it, so misses are
         * dropped and the Host falls back to the raw tool id.
         */
        const pushLabels = async () => {
          let locale = null
          try {
            locale = typeof ctx.get === 'function' ? ctx.get('locale') : null
          } catch {
            return
          }
          if (!locale || typeof locale.bind !== 'function') return
          let revision = null
          try {
            revision = locale.getSnapshot?.()?.revision ?? null
          } catch {
            revision = null
          }
          if (revision !== null && revision === sentRevision) return
          const t = locale.bind('chat')
          if (typeof t !== 'function') return

          const labels = {}
          const collect = (key, short) => {
            let text = null
            try {
              text = t(key)
            } catch {
              return
            }
            if (typeof text === 'string' && text && text !== key) labels[short] = text
          }
          for (const category of ACTIVITY_CATEGORIES) {
            collect(`message.stepProcess.${category}`, `stepProcess.${category}`)
          }
          for (const key of ['worked', 'failed']) {
            collect(`message.turnProcess.${key}`, `turnProcess.${key}`)
          }
          if (!Object.keys(labels).length) return
          try {
            const response = await fetch('/api-dsh-status/labels', {
              method: 'POST',
              headers: { 'content-type': 'application/json' },
              body: JSON.stringify({ labels }),
            })
            if (response.ok) sentRevision = revision
          } catch {
            // Retry on the next poll; the Host keeps its previous wording.
          }
        }

        const workspaceService = () => {
          try {
            const service = typeof ctx.get === 'function'
              ? ctx.get('uiWorkspace')
              : undefined
            return service ?? ctx.uiWorkspace ?? null
          } catch {
            return null
          }
        }

        const openSession = (sessionId) => {
          const service = workspaceService()
          if (!service || typeof service.openSession !== 'function') return false
          try {
            service.openSession(sessionId)
          } catch {
            return false
          }
          try {
            window.focus()
          } catch {}
          return true
        }

        const poll = async () => {
          if (disposed || busy) return
          busy = true
          try {
            await pushLabels()
            const response = await fetch('/api-dsh-status/pending', {
              headers: { accept: 'application/json' },
              cache: 'no-store',
            })
            if (!response.ok) return
            const data = await response.json()
            const sessionId = typeof data?.sessionId === 'string' ? data.sessionId : ''
            const at = Number.isFinite(data?.at) ? data.at : 0
            if (!sessionId || at === lastAt) return
            if (!openSession(sessionId)) return
            lastAt = at
            await fetch('/api-dsh-status/focus-done', {
              method: 'POST',
              headers: { 'content-type': 'application/json' },
              body: JSON.stringify({ sessionId, at }),
            })
          } catch {
            // The Host may still be booting, or the page may be backgrounded.
          } finally {
            busy = false
          }
        }

        const timer = setInterval(() => {
          void poll()
        }, POLL_MS)

        const stop = () => {
          disposed = true
          clearInterval(timer)
        }

        if (typeof ctx.effect === 'function') ctx.effect(() => stop)
        else if (typeof ctx.on === 'function') ctx.on('dispose', stop)

        void poll()
      },
    }
  },
})
