<script>
    import { onMount, onDestroy } from 'svelte';
    import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';
    import { Button } from '$lib/components/ui/button';
    import { availableUnassignedDevices } from '$lib/stores/settings';
    import { lptDevicePath } from '$lib/stores/settings';
    import { brakeTesterDevicePath } from '$lib/stores/settings';
    import { connectSettingsSocket } from '$lib/stores/settings';
    import { disconnectSettingsSocket } from '$lib/stores/settings';
    import { assignLptDevice } from '$lib/stores/settings';
    import { unassignLptDevice } from '$lib/stores/settings';
    import { assignBrakeTesterDevice } from '$lib/stores/settings';
    import { unassignBrakeTesterDevice } from '$lib/stores/settings';
    import { runLptTest1 } from '$lib/stores/settings';
    import { runLptTest2 } from '$lib/stores/settings';

    let selectedDevice = '';
    let lptSelection = '';
    let brakeTesterSelection = '';
    let lptOptions = [];
    let brakeTesterOptions = [];

    onMount(() => {
        connectSettingsSocket();
    });

    onDestroy(() => {
        disconnectSettingsSocket();
    });

    function assignToLpt() {
        assignLptDevice(selectedDevice);
        selectedDevice = '';
    }

    function assignToBrakeTester() {
        assignBrakeTesterDevice(selectedDevice);
        selectedDevice = '';
    }

    function handleLptSelectionChange(event) {
        const devicePath = event.currentTarget.value;
        if (!devicePath) {
            unassignLptDevice();
            return;
        }

        assignLptDevice(devicePath);
    }

    function handleBrakeTesterSelectionChange(event) {
        const devicePath = event.currentTarget.value;
        if (!devicePath) {
            unassignBrakeTesterDevice();
            return;
        }

        assignBrakeTesterDevice(devicePath);
    }

    function buildSelectionOptions(currentDevice) {
        const options = [...$availableUnassignedDevices];
        if (currentDevice && !options.includes(currentDevice)) {
            options.unshift(currentDevice);
        }
        return options;
    }

    $: lptOptions = buildSelectionOptions($lptDevicePath);
    $: brakeTesterOptions = buildSelectionOptions($brakeTesterDevicePath);
    $: lptSelection = $lptDevicePath || '';
    $: brakeTesterSelection = $brakeTesterDevicePath || '';

    $: if (selectedDevice && !$availableUnassignedDevices.includes(selectedDevice)) {
        selectedDevice = '';
    }
</script>

<Card class="w-full">
    <CardHeader>
        <CardTitle class="text-base text-foreground">Communication Hardware</CardTitle>
    </CardHeader>

    <CardContent class="space-y-4 text-foreground">
        <div class="grid gap-4 md:grid-cols-[minmax(0,1fr)_minmax(18rem,24rem)] md:items-start">
            <div class="flex h-full flex-col justify-center gap-3">
                <div class="grid gap-2 md:hidden">
                    <label for="lpt-device-select" class="text-sm text-muted-foreground">Lpt:</label>
                    <select
                        id="lpt-device-select"
                        class="h-9 w-full min-w-0 max-w-full rounded-md border border-border bg-muted px-2 text-sm text-foreground"
                        bind:value={lptSelection}
                        onchange={handleLptSelectionChange}
                    >
                        <option value="">Unassigned</option>
                        {#each lptOptions as devicePath}
                            <option value={devicePath}>{devicePath}</option>
                        {/each}
                    </select>
                </div>

                <div class="grid gap-2 md:hidden">
                    <label for="braketester-device-select" class="text-sm text-muted-foreground">BrakeTester:</label>
                    <select
                        id="braketester-device-select"
                        class="h-9 w-full min-w-0 max-w-full rounded-md border border-border bg-muted px-2 text-sm text-foreground"
                        bind:value={brakeTesterSelection}
                        onchange={handleBrakeTesterSelectionChange}
                    >
                        <option value="">Unassigned</option>
                        {#each brakeTesterOptions as devicePath}
                            <option value={devicePath}>{devicePath}</option>
                        {/each}
                    </select>
                </div>

                <div class="hidden grid-cols-[7rem_minmax(0,1fr)_auto_auto] items-center gap-2 md:grid">
                    <span class="text-sm text-muted-foreground">Lpt:</span>
                    <input class="h-9 min-w-0 rounded-md border border-border bg-muted px-2 text-sm text-foreground" readonly value={$lptDevicePath || 'Unassigned'} />
                    <Button class="shrink-0" variant="secondary" size="icon" onclick={assignToLpt} disabled={!selectedDevice} aria-label="Assign selected serial device to Lpt">&lt;</Button>
                    <Button class="shrink-0" variant="outline" size="icon" onclick={unassignLptDevice} disabled={!$lptDevicePath} aria-label="Unassign Lpt serial device">&gt;</Button>
                </div>

                <div class="hidden grid-cols-[7rem_minmax(0,1fr)_auto_auto] items-center gap-2 md:grid">
                    <span class="text-sm text-muted-foreground">BrakeTester:</span>
                    <input class="h-9 min-w-0 rounded-md border border-border bg-muted px-2 text-sm text-foreground" readonly value={$brakeTesterDevicePath || 'Unassigned'} />
                    <Button class="shrink-0" variant="secondary" size="icon" onclick={assignToBrakeTester} disabled={!selectedDevice} aria-label="Assign selected serial device to BrakeTester">&lt;</Button>
                    <Button class="shrink-0" variant="outline" size="icon" onclick={unassignBrakeTesterDevice} disabled={!$brakeTesterDevicePath} aria-label="Unassign BrakeTester serial device">&gt;</Button>
                </div>
            </div>

            <div class="hidden min-w-0 md:block">
                <label for="serial-device-list" class="mb-1 block text-sm text-muted-foreground">Available serial devices</label>
                <select
                    id="serial-device-list"
                    size="8"
                    class="w-full min-w-0 max-w-full rounded-md border border-border bg-muted p-2 text-sm text-foreground"
                    bind:value={selectedDevice}
                >
                    {#if $availableUnassignedDevices.length === 0}
                        <option disabled>(No unassigned serial devices found)</option>
                    {:else}
                        {#each $availableUnassignedDevices as devicePath}
                            <option value={devicePath}>{devicePath}</option>
                        {/each}
                    {/if}
                </select>
            </div>
        </div>

        <div class="border-t border-border pt-3">
            <div class="mb-2 text-sm font-medium text-foreground">Lpt Tests</div>
            <div class="flex flex-wrap gap-2">
                <Button variant="outline" onclick={runLptTest1} disabled={!$lptDevicePath}>Test 1</Button>
                <Button variant="outline" onclick={runLptTest2} disabled={!$lptDevicePath}>Test 2</Button>
            </div>
        </div>
    </CardContent>
</Card>
