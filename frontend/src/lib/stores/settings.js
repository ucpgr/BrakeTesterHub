import { writable, derived } from 'svelte/store';

export const serialDevices = writable([]);
export const lptDevicePath = writable('');
export const brakeTesterDevicePath = writable('');

let settingsSocket = null;

export const availableUnassignedDevices = derived(
    [serialDevices, lptDevicePath, brakeTesterDevicePath],
    ([$serialDevices, $lptDevicePath, $brakeTesterDevicePath]) =>
        $serialDevices.filter((path) => path !== $lptDevicePath && path !== $brakeTesterDevicePath)
);

function sendSettingsMessage(payload) {
    if (!settingsSocket || settingsSocket.readyState !== WebSocket.OPEN) return;
    settingsSocket.send(JSON.stringify(payload));
}

export function connectSettingsSocket() {
    if (settingsSocket && (settingsSocket.readyState === WebSocket.OPEN || settingsSocket.readyState === WebSocket.CONNECTING)) {
        return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    settingsSocket = new WebSocket(`${protocol}//${window.location.host}/api/settings`);

    settingsSocket.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            if (payload.event !== 'settings.state') return;

            serialDevices.set(Array.isArray(payload.serialDevices) ? payload.serialDevices : []);
            lptDevicePath.set(payload.lptDevicePath ?? '');
            brakeTesterDevicePath.set(payload.brakeTesterDevicePath ?? '');
        } catch (error) {
            console.warn('Failed to parse /api/settings message', error);
        }
    };

    settingsSocket.onclose = () => {
        settingsSocket = null;
        setTimeout(() => {
            connectSettingsSocket();
        }, 1000);
    };
}

export function disconnectSettingsSocket() {
    if (!settingsSocket) return;
    settingsSocket.close();
    settingsSocket = null;
}

export function assignLptDevice(devicePath) {
    if (!devicePath) return;
    sendSettingsMessage({ action: 'assign_lpt', devicePath });
}

export function unassignLptDevice() {
    sendSettingsMessage({ action: 'unassign_lpt' });
}

export function assignBrakeTesterDevice(devicePath) {
    if (!devicePath) return;
    sendSettingsMessage({ action: 'assign_braketester', devicePath });
}

export function unassignBrakeTesterDevice() {
    sendSettingsMessage({ action: 'unassign_braketester' });
}

export function runLptTest1() {
    sendSettingsMessage({ action: 'test_lpt', setTestEnabled: true });
}

export function runLptTest2() {
    sendSettingsMessage({ action: 'test_lpt', setTestEnabled: false });
}
