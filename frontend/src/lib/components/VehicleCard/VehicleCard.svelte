<script>
    import { vehicles, selectedVehicleId } from '$lib/stores/vehicles';
    import { testState } from '$lib/stores/results';

    import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';
    import { Select } from '$lib/components/ui/select';
    import { Button } from '$lib/components/ui/button';

    import { Plus, Trash2 } from 'lucide-svelte';

    import { get } from 'svelte/store';

    function selectVehicle(id) {
        selectedVehicleId.set(id);
    }

    function addVehicle() {
        // open modal or POST
    }

    function removeVehicle() {
        const id = get(selectedVehicleId);
        if (!id) return;

        // call DELETE /api/vehicles/:id
    }

    $: selected = $vehicles.find(v => v.id === $selectedVehicleId);
</script>

<Card class="w-full">
    <CardHeader>
        <CardTitle class="text-base">Vehicle</CardTitle>
    </CardHeader>

    <CardContent class="space-y-4">

        <!-- Dropdown + Icons -->
        <div class="flex gap-2 items-center">
            <select
                    class="flex-1 h-9 rounded-md border bg-background px-3 text-sm"
                    bind:value={$selectedVehicleId}
                    on:change={(e) => selectVehicle(e.target.value)}
            >
                <option value="">Select vehicle</option>
                {#each $vehicles as v}
                    <option value={v.id}>
                        {v.reg} — {v.make}
                    </option>
                {/each}
            </select>

            <Button size="icon" variant="secondary" on:click={addVehicle}>
                <Plus class="h-4 w-4" />
            </Button>

            <Button
                    size="icon"
                    variant="destructive"
                    disabled={!$selectedVehicleId || $testState === 'running'}
                    on:click={removeVehicle}
            >
                <Trash2 class="h-4 w-4" />
            </Button>
        </div>

        {#if selected}
            <div class="text-sm space-y-1">
                <div><span class="text-muted-foreground">Reg:</span> {selected.reg}</div>
                <div><span class="text-muted-foreground">Make:</span> {selected.make}</div>
                <div><span class="text-muted-foreground">Axles:</span> {selected.axles}</div>
            </div>
        {/if}

    </CardContent>
</Card>