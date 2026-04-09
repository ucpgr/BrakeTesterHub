<script>
    import { onMount } from 'svelte';
    import { get } from 'svelte/store';

    import { axleResults, testState } from '$lib/stores/results';
    import {
        VehicleListStore,
        SelectedVehicleStore,
        selectVehicle,
        removeVehicle
    } from '$lib/stores/vehicles';

    import { Card, CardHeader, CardTitle, CardContent, CardAction } from '$lib/components/ui/card';
    import { Badge } from '$lib/components/ui/badge';
    import { Button } from '$lib/components/ui/button';

    import { Plus, Printer, Trash2, X } from 'lucide-svelte';

    let cardRef;
    let cardWidth = 0;
    let showVehicleModal = false;

    const COMPACT_THRESHOLD = 760;
    const ICON_ONLY_THRESHOLD = 560;

    $: compactControls = cardWidth < COMPACT_THRESHOLD;
    $: iconOnlyControls = cardWidth < ICON_ONLY_THRESHOLD;

    $: hasSelection = !!$SelectedVehicleStore;

    onMount(() => {
        const resizeObserver = new ResizeObserver(([entry]) => {
            cardWidth = entry.contentRect.width;
        });

        if (cardRef) {
            resizeObserver.observe(cardRef);
        }

        return () => resizeObserver.disconnect();
    });

    function onVehicleChange(event) {
        selectVehicle(event.currentTarget.value || null);
    }

    function onAddVehicle() {
        // TODO: wire to create-vehicle flow
        showVehicleModal = true;
    }

    async function onRemoveVehicle() {
        const id = get(SelectedVehicleStore);
        if (!id || $testState === 'running') return;
        await removeVehicle(id);
    }
</script>

<Card class="w-full" bind:ref={cardRef}>
    <CardHeader class="grid-cols-[1fr_auto] items-center gap-3 pb-4">
        <CardTitle class="text-base">Test Results</CardTitle>

        <CardAction class="row-span-1 row-start-1 self-center">
            {#if iconOnlyControls}
                <Button size="icon" variant="secondary" aria-label="Vehicle controls" on:click={() => (showVehicleModal = true)}>
                    <Printer class="h-4 w-4" />
                </Button>
            {:else}
                <div class="flex items-center gap-2 min-w-0">
                    <Button size="icon" variant="secondary" aria-label="Print options" on:click={() => (showVehicleModal = true)}>
                        <Printer class="h-4 w-4" />
                    </Button>

                    <span class="text-xs text-muted-foreground whitespace-nowrap {compactControls ? 'hidden' : ''}">
                        Next test:
                    </span>

                    <select
                        class="h-9 min-w-0 w-[220px] max-w-[42vw] rounded-md border bg-background px-3 text-sm transition-colors focus-visible:border-ring focus-visible:ring-ring/50 focus-visible:ring-[3px] {hasSelection ? 'border-emerald-400/70 ring-1 ring-emerald-300/40' : 'border-input'}"
                        bind:value={$SelectedVehicleStore}
                        on:change={onVehicleChange}
                    >
                        <option value="">No vehicle</option>
                        {#each $VehicleListStore as vehicle}
                            <option value={vehicle.id}>{vehicle.reg} — {vehicle.make}</option>
                        {/each}
                    </select>

                    <Button size="icon" variant="secondary" aria-label="Add vehicle" on:click={onAddVehicle}>
                        <Plus class="h-4 w-4" />
                    </Button>

                    <Button
                        size="icon"
                        variant="destructive"
                        aria-label="Remove vehicle"
                        disabled={!$SelectedVehicleStore || $testState === 'running'}
                        on:click={onRemoveVehicle}
                    >
                        <Trash2 class="h-4 w-4" />
                    </Button>
                </div>
            {/if}
        </CardAction>
    </CardHeader>

    <CardContent>

        <!-- Header Row -->
        <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] text-xs text-muted-foreground pb-2 border-b">
            <div>Axle</div>
            <div class="text-right">Left</div>
            <div class="text-right">Right</div>
            <div class="text-right">Weight</div>
            <div class="text-right">Imbal</div>
            <div class="text-center">Status</div>
        </div>

        {#each $axleResults as axle}
            <div class="py-3 border-b last:border-0">

                <!-- Main Row -->
                <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] items-center">

                    <div class="font-medium">
                        {axle.axle}
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.leftForce} kgf
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.rightForce} kgf
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.weight} kg
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.imbalance.toFixed(1)}%
                    </div>

                    <div class="text-center">
                        <Badge variant={axle.status === 'PASS' ? 'default' : 'destructive'}>
                            {axle.status}
                        </Badge>
                    </div>

                </div>

                <!-- Efficiency Row -->
                <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] text-xs text-muted-foreground mt-1">

                    <div></div>

                    <div class="text-right tabular-nums">
                        {axle.leftEff}%
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.rightEff}%
                    </div>

                    <div></div>
                    <div></div>
                    <div></div>

                </div>

            </div>
        {/each}

    </CardContent>
</Card>

{#if showVehicleModal}
    <div class="fixed inset-0 z-50 flex items-center justify-center bg-black/40 p-4" role="presentation">
        <div class="w-full max-w-md rounded-lg border bg-background p-4 shadow-lg">
            <div class="mb-4 flex items-center justify-between">
                <h3 class="text-sm font-semibold">Vehicle controls</h3>
                <Button size="icon" variant="ghost" aria-label="Close vehicle controls" on:click={() => (showVehicleModal = false)}>
                    <X class="h-4 w-4" />
                </Button>
            </div>

            <div class="space-y-3">
                <label class="text-xs text-muted-foreground" for="vehicle-picker-modal">Next test</label>
                <select
                    id="vehicle-picker-modal"
                    class="h-9 w-full rounded-md border bg-background px-3 text-sm transition-colors focus-visible:border-ring focus-visible:ring-ring/50 focus-visible:ring-[3px] {hasSelection ? 'border-emerald-400/70 ring-1 ring-emerald-300/40' : 'border-input'}"
                    bind:value={$SelectedVehicleStore}
                    on:change={onVehicleChange}
                >
                    <option value="">No vehicle</option>
                    {#each $VehicleListStore as vehicle}
                        <option value={vehicle.id}>{vehicle.reg} — {vehicle.make}</option>
                    {/each}
                </select>

                <div class="flex justify-end gap-2 pt-1">
                    <Button variant="secondary" on:click={onAddVehicle}>
                        <Plus class="mr-2 h-4 w-4" /> Add
                    </Button>
                    <Button
                        variant="destructive"
                        disabled={!$SelectedVehicleStore || $testState === 'running'}
                        on:click={onRemoveVehicle}
                    >
                        <Trash2 class="mr-2 h-4 w-4" /> Remove
                    </Button>
                </div>
            </div>
        </div>
    </div>
{/if}
