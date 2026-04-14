<script>
    import { onMount } from 'svelte';
    import { get } from 'svelte/store';

    import { axleResults, testState } from '$lib/stores/results';
    import {
        VehicleListStore,
        SelectedVehicleStore,
        selectedVehicle,
        connectVehicleSocket,
        disconnectVehicleSocket,
        addVehicle,
        selectVehicle,
        removeVehicle
    } from '$lib/stores/vehicles';

    import { Card, CardHeader, CardTitle, CardContent, CardAction } from '$lib/components/ui/card';
    import { Badge } from '$lib/components/ui/badge';
    import { Button } from '$lib/components/ui/button';

    import { Plus, Trash2, X } from 'lucide-svelte';

    let cardRef;
    let cardWidth = 0;
    let showVehicleModal = false;
    let regInput = '';
    let makeInput = '';
    let modelInput = '';
    let mileageInput = 0;
    let mileageUnitInput = 'km';

    const COMPACT_THRESHOLD = 760;
    const ICON_ONLY_THRESHOLD = 560;

    $: compactControls = cardWidth < COMPACT_THRESHOLD;
    $: iconOnlyControls = cardWidth < ICON_ONLY_THRESHOLD;

    $: hasSelection = !!$SelectedVehicleStore;

    onMount(() => {
        connectVehicleSocket();

        const resizeObserver = new ResizeObserver(([entry]) => {
            cardWidth = entry.contentRect.width;
        });

        if (cardRef) {
            resizeObserver.observe(cardRef);
        }

        return () => {
            resizeObserver.disconnect();
            disconnectVehicleSocket();
        };
    });

    $: if ($selectedVehicle) {
        mileageInput = Number($selectedVehicle.mileage ?? 0);
        mileageUnitInput = $selectedVehicle.mileageUnit ?? 'km';
    }

    function onVehicleChange(event) {
        selectVehicle(event.currentTarget.value || null);
    }

    function openVehicleModal() {
        regInput = '';
        makeInput = '';
        modelInput = '';
        mileageInput = 0;
        mileageUnitInput = 'km';
        showVehicleModal = true;
    }

    function closeVehicleModal() {
        showVehicleModal = false;
    }

    function onAddVehicle() {
        if (!regInput.trim() || !makeInput.trim() || !modelInput.trim()) return;

        addVehicle({
            reg: regInput.trim(),
            make: makeInput.trim(),
            model: modelInput.trim(),
            mileage: Number(mileageInput) || 0,
            mileageUnit: mileageUnitInput
        });

        closeVehicleModal();
    }

    async function onRemoveVehicle() {
        const id = get(SelectedVehicleStore);
        if (!id || $testState === 'running') return;
        removeVehicle(id);
    }

</script>

<Card class="w-full" bind:ref={cardRef}>
    <CardHeader class="grid-cols-[1fr_auto] items-center gap-3 pb-4">
        <CardTitle class="text-base">Test Results</CardTitle>

        <CardAction class="row-span-1 row-start-1 self-center">
            {#if iconOnlyControls}
                <Button size="icon" variant="secondary" aria-label="Add vehicle" onclick={openVehicleModal}>
                    <Plus class="h-4 w-4" />
                </Button>
            {:else}
                <div class="flex items-center gap-2 min-w-0">
                    <span class="text-xs text-muted-foreground whitespace-nowrap {compactControls ? 'hidden' : ''}">
                        Next test:
                    </span>

                    <select
                        class="h-9 min-w-0 w-[220px] max-w-[42vw] rounded-md border bg-background px-3 text-sm transition-colors focus-visible:border-ring focus-visible:ring-ring/50 focus-visible:ring-[3px] {hasSelection ? 'border-emerald-400/70 ring-1 ring-emerald-300/40' : 'border-input'}"
                        bind:value={$SelectedVehicleStore}
                        onchange={onVehicleChange}
                    >
                        <option value="">No vehicle</option>
                        {#each $VehicleListStore as vehicle}
                            <option value={vehicle.id}>{vehicle.reg} — {vehicle.make}</option>
                        {/each}
                    </select>

                    <div class="flex items-center gap-2">
                        <input
                            type="number"
                            min="0"
                            class="h-9 w-[86px] rounded-md border bg-background px-2 text-sm"
                            bind:value={mileageInput}
                        />
                        <div class="inline-flex items-center rounded-full border bg-muted p-0.5">
                            <button
                                type="button"
                                class="h-7 min-w-[2.25rem] rounded-full px-2 text-xs font-medium transition-colors {mileageUnitInput === 'km' ? 'bg-background shadow-sm' : 'text-muted-foreground'}"
                                onclick={() => (mileageUnitInput = 'km')}
                            >
                                km
                            </button>
                            <button
                                type="button"
                                class="h-7 min-w-[2.25rem] rounded-full px-2 text-xs font-medium transition-colors {mileageUnitInput === 'm' ? 'bg-background shadow-sm' : 'text-muted-foreground'}"
                                onclick={() => (mileageUnitInput = 'm')}
                            >
                                m
                            </button>
                        </div>
                    </div>

                    <Button size="icon" variant="secondary" aria-label="Add vehicle" onclick={openVehicleModal}>
                        <Plus class="h-4 w-4" />
                    </Button>

                    <Button
                        size="icon"
                        variant="destructive"
                        aria-label="Remove vehicle"
                        disabled={!$SelectedVehicleStore || $testState === 'running'}
                        onclick={onRemoveVehicle}
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
                <h3 class="text-sm font-semibold">Add vehicle</h3>
                <Button size="icon" variant="ghost" aria-label="Close add vehicle dialog" onclick={closeVehicleModal}>
                    <X class="h-4 w-4" />
                </Button>
            </div>

            <div class="space-y-3">
                <div class="grid grid-cols-2 gap-2">
                    <input class="h-9 rounded-md border bg-background px-3 text-sm" placeholder="Reg" bind:value={regInput} />
                    <input class="h-9 rounded-md border bg-background px-3 text-sm" placeholder="Make" bind:value={makeInput} />
                    <input class="h-9 rounded-md border bg-background px-3 text-sm col-span-2" placeholder="Model" bind:value={modelInput} />
                </div>

                <div class="flex items-center gap-2 pt-1">
                    <input
                        type="number"
                        min="0"
                        class="h-9 w-28 rounded-md border bg-background px-3 text-sm"
                        bind:value={mileageInput}
                    />
                    <div class="inline-flex items-center rounded-full border bg-muted p-0.5">
                        <button
                            type="button"
                            class="h-7 min-w-[2.25rem] rounded-full px-2 text-xs font-medium transition-colors {mileageUnitInput === 'km' ? 'bg-background shadow-sm' : 'text-muted-foreground'}"
                            onclick={() => (mileageUnitInput = 'km')}
                        >
                            km
                        </button>
                        <button
                            type="button"
                            class="h-7 min-w-[2.25rem] rounded-full px-2 text-xs font-medium transition-colors {mileageUnitInput === 'm' ? 'bg-background shadow-sm' : 'text-muted-foreground'}"
                            onclick={() => (mileageUnitInput = 'm')}
                        >
                            m
                        </button>
                    </div>
                </div>

                <div class="flex justify-end gap-2 pt-3">
                    <Button variant="outline" onclick={closeVehicleModal}>Cancel</Button>
                    <Button variant="secondary" onclick={onAddVehicle}>Save</Button>
                </div>
            </div>
        </div>
    </div>
{/if}
