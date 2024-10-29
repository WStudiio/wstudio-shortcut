export type EventType = 'KeyDown' | 'KeyUp' | 'MouseDown' | 'MouseUp';

export interface ShortcutEvent {
    type: EventType;
    code: number;
}

export type ShortcutCallback = (event: ShortcutEvent) => void;

/**
 * Inicia a escuta de eventos de teclado e mouse.
 * @param callback Callback que será chamado para tratar eventos.
 * @param specificKey Tecla específica para monitorar (0 para todas as teclas).
 */
export function startListening(callback: ShortcutCallback, specificKey?: number): void;

/**
 * Para a escuta de eventos de teclado e mouse.
 */
export function stopListening(): void;
