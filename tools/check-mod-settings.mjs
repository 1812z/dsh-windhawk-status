/**
 * Validates the mod's Windhawk settings block and its C++ readers.
 *
 * Catches the three mistakes that fail silently instead of loudly:
 *   - YAML that does not parse (Windhawk shows an empty settings page)
 *   - a localized `$options:<lang>` list whose keys differ from `$options`
 *   - a setting declared in YAML but never read by Wh_Get*Setting, or read
 *     under a name that was never declared (the setting does nothing)
 *
 *   node tools/check-mod-settings.mjs
 */

import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const modPath = path.join(root, 'windhawk', 'dsh-taskbar-status.wh.cpp')
const source = fs.readFileSync(modPath, 'utf8')

// js-yaml is not a dependency of this package; borrow the app's copy.
const yamlCandidates = [
  path.join(root, 'node_modules', 'js-yaml', 'index.js'),
  'C:/Users/1812z/AppData/Local/Programs/DSH Desktop/resources/app/node_modules/js-yaml/index.js',
  'C:/Users/1812z/.dsh/profiles/desktop/node_modules/js-yaml/index.js',
]
let yaml = null
for (const candidate of yamlCandidates) {
  if (fs.existsSync(candidate)) {
    yaml = (await import(pathToFileURL(candidate).href)).default
    break
  }
}
if (!yaml) {
  console.error('js-yaml not found; checked:\n  ' + yamlCandidates.join('\n  '))
  process.exit(1)
}

const problems = []

// ------------------------------------------------------------------ YAML block

const blockMatch = source.match(
  /\/\/ ==WindhawkModSettings==\r?\n\/\*([\s\S]*?)\*\/\r?\n\/\/ ==\/WindhawkModSettings==/,
)
if (!blockMatch) {
  console.error('no ==WindhawkModSettings== block found')
  process.exit(1)
}

let settings
try {
  settings = yaml.load(blockMatch[1])
} catch (error) {
  console.error('settings YAML does not parse:\n  ' + error.message)
  process.exit(1)
}
if (!Array.isArray(settings) || settings.length === 0) {
  console.error('settings block is not a non-empty YAML array')
  process.exit(1)
}

const declared = new Set()
for (const entry of settings) {
  if (typeof entry !== 'object' || entry === null || Array.isArray(entry)) {
    problems.push(`entry is not a mapping: ${JSON.stringify(entry)}`)
    continue
  }
  const keys = Object.keys(entry).filter((key) => !key.startsWith('$'))
  if (keys.length !== 1) {
    problems.push(`entry must declare exactly one setting key, got ${JSON.stringify(keys)}`)
    continue
  }
  const key = keys[0]
  declared.add(key)
  if (!entry.$name) problems.push(`${key}: missing $name`)

  const baseOptions = entry.$options
  if (baseOptions !== undefined) {
    if (!Array.isArray(baseOptions)) {
      problems.push(`${key}: $options must be a list`)
    } else {
      const baseKeys = baseOptions.flatMap((option) => Object.keys(option)).sort()
      for (const [localizedKey, localizedValue] of Object.entries(entry)) {
        if (!localizedKey.startsWith('$options:')) continue
        if (!Array.isArray(localizedValue)) {
          problems.push(`${key}: ${localizedKey} must be a list`)
          continue
        }
        const localizedKeys = localizedValue.flatMap((option) => Object.keys(option)).sort()
        if (localizedKeys.join(',') !== baseKeys.join(',')) {
          problems.push(
            `${key}: ${localizedKey} keys [${localizedKeys}] != $options keys [${baseKeys}]`,
          )
        }
      }
    }
  }
}

// --------------------------------------------------------------- C++ readers

const read = new Set()
for (const match of source.matchAll(
  /(?:Wh_Get(?:Int|String)Setting|GetStringSetting)\(\s*L"([^"]+)"/g,
)) {
  read.add(match[1])
}

for (const key of declared) {
  if (!read.has(key)) problems.push(`${key}: declared in YAML but never read by Wh_Get*Setting`)
}
for (const key of read) {
  if (!declared.has(key)) problems.push(`${key}: read by Wh_Get*Setting but not declared in YAML`)
}

// ------------------------------------------------------------------- report

console.log(`${path.relative(root, modPath)}`)
console.log(`${settings.length} settings, ${read.size} C++ readers\n`)
for (const entry of settings) {
  const key = Object.keys(entry).find((name) => !name.startsWith('$'))
  const localized = Object.keys(entry).filter((name) => name.startsWith('$name:')).length
  const options = entry.$options ? ` options=${entry.$options.length}` : ''
  const kind = typeof entry[key]
  console.log(`  ${key.padEnd(20)} ${kind.padEnd(8)}${options}${localized ? '  zh-CN' : ''}`)
}

console.log()
if (problems.length) {
  console.log('FAIL')
  for (const problem of problems) console.log(`  - ${problem}`)
  process.exit(1)
}
console.log('PASS')
