// QuickJSのInfinity定義を変更しても、正負の無限大と属性が変わらないことを検証する。
if (Infinity !== 1 / 0 || -Infinity !== -1 / 0) throw Error('Infinity value mismatch');
if (1 / Infinity !== 0 || !Object.is(1 / -Infinity, -0)) throw Error('Infinity reciprocal mismatch');
if (isFinite(Infinity) || !Number.isNaN(Infinity - Infinity)) throw Error('Infinity arithmetic mismatch');
const descriptor = Object.getOwnPropertyDescriptor(globalThis, 'Infinity');
if (descriptor.writable || descriptor.configurable || descriptor.enumerable) throw Error('Infinity descriptor mismatch');
drawing.line(0, 0, 1, 1);
