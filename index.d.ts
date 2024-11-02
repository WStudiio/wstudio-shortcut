// index.d.ts
export type EventType = 'KeyDown' | 'KeyUp' | 'MouseDown' | 'MouseUp';

export interface EventData {
    type: EventType;
    code: number;
}

/**
 * Inicia a escuta de eventos de teclado e mouse.
 * @param callback Callback para tratar eventos.
 * @param specificKey (Opcional) Tecla específica para monitorar (0 para todas as teclas).
 */
export function startListening(callback: (event: EventData) => void, specificKey?: number): void;

/**
 * Para a escuta de eventos de teclado e mouse.
 */
export function stopListening(): void;
