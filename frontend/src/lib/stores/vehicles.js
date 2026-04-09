import { writable, derived, get } from 'svelte/store';

/* =========================================================
   Core Stores
========================================================= */

export const VehicleListStore = writable([]);
export const SelectedVehicleStore = writable(null);

// Backwards-compatible aliases
export const vehicles = VehicleListStore;
export const selectedVehicleId = SelectedVehicleStore;

/* =========================================================
   Derived
========================================================= */

export const selectedVehicle = derived(
    [VehicleListStore, SelectedVehicleStore],
    ([$vehicles, $selectedVehicleId]) =>
        $vehicles.find(v => v.id === $selectedVehicleId) ?? null
);

/* =========================================================
   Internal Helpers
========================================================= */

async function api(url, options = {}) {
    const res = await fetch(url, {
        headers: { 'Content-Type': 'application/json' },
        cache: 'no-store',
        ...options
    });

    if (!res.ok) {
        const text = await res.text();
        throw new Error(`API error (${res.status}): ${text}`);
    }

    return res.status === 204 ? null : res.json();
}

/* =========================================================
   Public API
========================================================= */

/**
 * Load vehicles from server
 */
export async function loadVehicles() {
    const data = await api('/api/vehicles');
    VehicleListStore.set(data);

    const currentSelected = get(SelectedVehicleStore);
    if (currentSelected && !data.some((v) => v.id === currentSelected)) {
        SelectedVehicleStore.set(null);
    }
}

/**
 * Add vehicle
 * vehicle = { reg, make, axles }
 */
export async function addVehicle(vehicle) {
    const updated = await api('/api/vehicles', {
        method: 'POST',
        body: JSON.stringify(vehicle)
    });

    VehicleListStore.set(updated);
}

/**
 * Update vehicle
 */
export async function updateVehicle(id, vehicle) {
    const updated = await api(`/api/vehicles/${id}`, {
        method: 'PUT',
        body: JSON.stringify(vehicle)
    });

    VehicleListStore.set(updated);
}

/**
 * Remove vehicle
 */
export async function removeVehicle(id) {
    await api(`/api/vehicles/${id}`, {
        method: 'DELETE'
    });

    const current = get(VehicleListStore).filter(v => v.id !== id);
    VehicleListStore.set(current);

    if (get(SelectedVehicleStore) === id) {
        SelectedVehicleStore.set(null);
    }
}

/**
 * Select vehicle
 */
export function selectVehicle(id) {
    SelectedVehicleStore.set(id || null);
}
