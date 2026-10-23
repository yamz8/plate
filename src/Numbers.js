.pragma library

// Numbers in the language's own format, shared by the sidebar and the dilution dialog.

function format(locale, value) {
    return Number(value).toLocaleString(locale, 'g', 6)
}

// Accepts the language's decimal separator, and a plain point too. A point is
// always a decimal point here, never a thousands separator, so "1.500" is 1.5
// in Danish rather than 1500. Returns NaN when the text isn't a number.
function parse(locale, text) {
    const plain = text.trim().replace(locale.decimalPoint, ".")
    if (/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(plain))
        return Number(plain)
    // Digits other than 0–9, as some languages write them.
    try {
        return Number.fromLocaleString(locale, text.trim())
    } catch (e) {
        return NaN
    }
}
