import { writable } from 'svelte/store';

export const isOnline = writable(false);

const IDLE_STATUS = {
    level: 'idle',
    text: 'Idle'
};

export const status = writable(IDLE_STATUS);

let idleTimer = null;
const IDLE_TIMEOUT_MS = 10000;

/**
 * Set a new status and automatically revert to idle
 */
export function setStatus(next) {
    status.set(next);

    // reset idle timer
    if (idleTimer) {
        clearTimeout(idleTimer);
    }

    idleTimer = setTimeout(() => {
        status.set(IDLE_STATUS);
    }, IDLE_TIMEOUT_MS);
}

// Initialise explicitly at startup
setStatus(IDLE_STATUS);