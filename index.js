const path = require('path');

// Definindo o caminho para o módulo nativo .node
const addonPath = path.join(__dirname, 'build/Release/shortcut.node');
const shortcut = require(addonPath);

/**
 * Inicia a escuta de eventos de teclado e mouse.
 * @param {Function} callback Callback para tratar eventos.
 * @param {number} specificKey Tecla específica para monitorar (0 para todas as teclas).
 */
function startListening(callback, specificKey = 0) {
    shortcut.startListening((type, code) => {
        const eventType = ['KeyDown', 'KeyUp', 'MouseDown', 'MouseUp'][type];
        callback({ type: eventType, code });
    }, specificKey);
}

/**
 * Para a escuta de eventos de teclado e mouse.
 */
function stopListening() {
    shortcut.stopListening();
}

module.exports = {
    startListening,
    stopListening
};
