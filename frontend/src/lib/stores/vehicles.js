import { writable, derived, get } from 'svelte/store';

/* =========================================================
   Core Stores
========================================================= */

export const vehicles = writable([]);
export const selectedVehicleId = writable(null);

/* =========================================================
   Derived
========================================================= */

export const selectedVehicle = derived(
    [vehicles, selectedVehicleId],
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
    vehicles.set(data);

    // auto-select first if none selected
    if (!get(selectedVehicleId) && data.length > 0) {
        selectedVehicleId.set(data[0].id);
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

    vehicles.set(updated);
}

/**
 * Update vehicle
 */
export async function updateVehicle(id, vehicle) {
    const updated = await api(`/api/vehicles/${id}`, {
        method: 'PUT',
        body: JSON.stringify(vehicle)
    });

    vehicles.set(updated);
}

/**
 * Remove vehicle
 */
export async function removeVehicle(id) {
    await api(`/api/vehicles/${id}`, {
        method: 'DELETE'
    });

    const current = get(vehicles).filter(v => v.id !== id);
    vehicles.set(current);

    if (get(selectedVehicleId) === id) {
        selectedVehicleId.set(current.length ? current[0].id : null);
    }
}

/**
 * Select vehicle
 */
export function selectVehicle(id) {
    selectedVehicleId.set(id);
}