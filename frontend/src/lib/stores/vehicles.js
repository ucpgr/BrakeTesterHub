import { writable, derived, get } from 'svelte/store';

export const VehicleListStore = writable([]);
export const SelectedVehicleStore = writable(null);

let vehicleSocket = null;

export const selectedVehicle = derived(
    [VehicleListStore, SelectedVehicleStore],
    ([$vehicles, $selectedVehicleId]) =>
        $vehicles.find((vehicle) => vehicle.id === Number($selectedVehicleId)) ?? null
);

function sendSocketMessage(payload) {
    if (!vehicleSocket || vehicleSocket.readyState !== WebSocket.OPEN) return;
    vehicleSocket.send(JSON.stringify(payload));
}

export function connectVehicleSocket() {
    if (vehicleSocket && (vehicleSocket.readyState === WebSocket.OPEN || vehicleSocket.readyState === WebSocket.CONNECTING)) {
        return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    vehicleSocket = new WebSocket(`${protocol}//${window.location.host}/api/vehicles`);

    vehicleSocket.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            if (payload.event !== 'vehicles.state') return;

            VehicleListStore.set(Array.isArray(payload.vehicles) ? payload.vehicles : []);
            SelectedVehicleStore.set(payload.selectedVehicleId ?? null);
        } catch (error) {
            console.warn('Failed to parse /api/vehicles message', error);
        }
    };

    vehicleSocket.onclose = () => {
        vehicleSocket = null;
        setTimeout(() => {
            connectVehicleSocket();
        }, 1000);
    };
}

export function disconnectVehicleSocket() {
    if (!vehicleSocket) return;
    vehicleSocket.close();
    vehicleSocket = null;
}

export function addVehicle(vehicle) {
    sendSocketMessage({ action: 'add', vehicle });
}

export function removeVehicle(id) {
    sendSocketMessage({ action: 'delete', id: Number(id) });
}

export function updateVehicleMileage(id, mileage) {
    const parsedId = Number(id);
    if (!parsedId || Number.isNaN(parsedId)) return;

    sendSocketMessage({
        action: 'update_mileage',
        id: parsedId,
        mileage: mileage && mileage.trim().length > 0 ? mileage.trim() : null
    });
}

export function selectVehicle(id) {
    const parsedId = id ? Number(id) : null;
    SelectedVehicleStore.set(parsedId);

    if (parsedId === null || Number.isNaN(parsedId)) {
        sendSocketMessage({ action: 'select' });
        return;
    }

    sendSocketMessage({ action: 'select', id: parsedId });
}

export const vehicles = VehicleListStore;
export const selectedVehicleId = SelectedVehicleStore;

export function getCurrentSelectedVehicle() {
    return get(selectedVehicle);
}
