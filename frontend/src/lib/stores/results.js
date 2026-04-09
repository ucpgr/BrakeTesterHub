import { writable, derived, get } from 'svelte/store';

/* =========================================================
   Core Stores
========================================================= */

// Array of axle result objects
export const axleResults = writable([]);

// Test lifecycle
export const testState = writable('idle');
// 'idle' | 'running' | 'complete'

/* =========================================================
   Derived
========================================================= */

// Sorted by axle number (defensive safety)
export const sortedAxleResults = derived(
    axleResults,
    ($axleResults) =>
        [...$axleResults].sort((a, b) => a.axle - b.axle)
);

// Overall test status
export const overallStatus = derived(
    axleResults,
    ($axleResults) => {
        if ($axleResults.length === 0) return null;
        return $axleResults.some(a => a.status === 'FAIL')
            ? 'FAIL'
            : 'PASS';
    }
);

/* =========================================================
   Mutation API (WS Driven)
========================================================= */

/**
 * Called when test starts
 */
export function startTest() {
    axleResults.set([]);
    testState.set('running');
}

/**
 * Called when an axle result message arrives
 * result = {
 *   axle,
 *   leftForce,
 *   rightForce,
 *   weight,
 *   leftEff,
 *   rightEff,
 *   imbalance,
 *   status
 * }
 */
export function addAxleResult(result) {
    axleResults.update(current => {
        // prevent duplicates for same axle
        const filtered = current.filter(r => r.axle !== result.axle);
        return [...filtered, result];
    });
}

/**
 * Called when server sends testComplete
 */
export function completeTest() {
    testState.set('complete');
}

/**
 * Manual reset (optional)
 */
export function resetResults() {
    axleResults.set([]);
    testState.set('idle');
}