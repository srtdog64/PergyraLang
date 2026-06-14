// Runs a wasm32-wasi build of the Pergyra reactive demo on node's WASI.
// Usage: node --no-warnings run_wasi.mjs demo.wasm
import { WASI } from 'node:wasi';
import { readFile } from 'node:fs/promises';

const path = process.argv[2] || 'demo.wasm';
const wasi = new WASI({ version: 'preview1', args: ['demo'], env: {}, returnOnExit: true });
const bytes = await readFile(path);
const wasm = await WebAssembly.compile(bytes);
const instance = await WebAssembly.instantiate(wasm, wasi.getImportObject());
wasi.start(instance);
