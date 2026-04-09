import { writable, derived } from 'svelte/store';

/* ================= LIVE TELEMETRY ================= */

export const telemetry = writable({
    leftForce: 0,
    rightForce: 0,
    axleWeight: 0,

    leftRollerActive: false,
    rightRollerActive: false,
    leftWheelDetected: false,
    rightWheelDetected: false
});

/* ================= PEAK HOLD ================= */

export const peaks = writable({
    left: 0,
    right: 0
});

/* ================= DERIVED ================= */

export const efficiencies = derived(
    telemetry,
    ($t) => {
        if ($t.axleWeight <= 0) {
            return { left: null, right: null };
        }

        return {
            left: Math.round(($t.leftForce / $t.axleWeight) * 100),
            right: Math.round(($t.rightForce / $t.axleWeight) * 100)
        };
    }
);

export const imbalance = derived(
    telemetry,
    ($t) => {
        const max = Math.max($t.leftForce, $t.rightForce);
        if (max === 0) return 0;

        return ((($t.leftForce - $t.rightForce) / max) * 100);
    }
);