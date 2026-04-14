import { writable } from 'svelte/store';

export const isOnline = writable(false);

const IDLE_STATUS = {
    level: 'idle',
    text: 'Idle'
};

export const status = writable(IDLE_STATUS);

let idleTimer = null;
const IDLE_TIMEOUT_MS = 10000;
let statusSocket = null;

/**
 * Set a new status and automatically revert to idle.
 */
export function setStatus(next) {
    status.set(next);

    if (idleTimer) {
        clearTimeout(idleTimer);
    }

    idleTimer = setTimeout(() => {
        status.set(IDLE_STATUS);
    }, IDLE_TIMEOUT_MS);
}

function sendIdleStatus() {
    status.set(IDLE_STATUS);
}

export function connectStatusSocket() {
    if (statusSocket && (statusSocket.readyState === WebSocket.OPEN || statusSocket.readyState === WebSocket.CONNECTING)) {
        return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    statusSocket = new WebSocket(`${protocol}//${window.location.host}/status`);

    statusSocket.onopen = () => {
        isOnline.set(true);
    };

    statusSocket.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            if (payload.event !== 'status.update') return;

            const nextStatus = payload.status;
            if (!nextStatus || typeof nextStatus.text !== 'string' || typeof nextStatus.level !== 'string') {
                return;
            }

            setStatus({
                level: nextStatus.level,
                text: nextStatus.text
            });
        } catch (error) {
            console.warn('Failed to parse /status message', error);
        }
    };

    statusSocket.onclose = () => {
        isOnline.set(false);
        statusSocket = null;
        sendIdleStatus();

        setTimeout(() => {
            connectStatusSocket();
        }, 1000);
    };
}

export function disconnectStatusSocket() {
    if (!statusSocket) return;
    statusSocket.close();
    statusSocket = null;
}

setStatus(IDLE_STATUS);
