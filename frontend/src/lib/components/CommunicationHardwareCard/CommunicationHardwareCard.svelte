<script>
    import { onMount, onDestroy } from 'svelte';
    import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';
    import { Button } from '$lib/components/ui/button';
    import {
        availableUnassignedDevices,
        selectedSerialDevice,
        lptDevicePath,
        brakeTesterDevicePath,
        connectSettingsSocket,
        disconnectSettingsSocket,
        selectSerialDevice,
        assignLptDevice,
        unassignLptDevice,
        assignBrakeTesterDevice,
        unassignBrakeTesterDevice,
        runLptTest1,
        runLptTest2
    } from '$lib/stores/settings';

    onMount(() => {
        connectSettingsSocket();
    });

    onDestroy(() => {
        disconnectSettingsSocket();
    });

    function assignToLpt() {
        assignLptDevice($selectedSerialDevice);
        selectSerialDevice('');
    }

    function assignToBrakeTester() {
        assignBrakeTesterDevice($selectedSerialDevice);
        selectSerialDevice('');
    }
</script>

<Card class="w-full">
    <CardHeader>
        <CardTitle class="text-base text-foreground">Communication Hardware</CardTitle>
    </CardHeader>

    <CardContent class="space-y-4 text-foreground">
        <div class="grid gap-4 md:grid-cols-[1fr_auto_1fr]">
            <div class="space-y-3">
                <div class="grid grid-cols-[7rem_1fr_auto] items-center gap-2">
                    <span class="text-sm text-muted-foreground">Lpt:</span>
                    <input class="h-9 rounded-md border border-border bg-muted px-2 text-sm text-foreground" readonly value={$lptDevicePath || 'Unassigned'} />
                    <Button variant="secondary" size="icon" on:click={assignToLpt} disabled={!$selectedSerialDevice} aria-label="Assign selected serial device to Lpt">&lt;</Button>
                </div>

                <div class="grid grid-cols-[7rem_1fr_auto] items-center gap-2">
                    <span class="text-sm text-muted-foreground">BrakeTester:</span>
                    <input class="h-9 rounded-md border border-border bg-muted px-2 text-sm text-foreground" readonly value={$brakeTesterDevicePath || 'Unassigned'} />
                    <Button variant="secondary" size="icon" on:click={assignToBrakeTester} disabled={!$selectedSerialDevice} aria-label="Assign selected serial device to BrakeTester">&lt;</Button>
                </div>
            </div>

            <div class="flex flex-col gap-2 justify-center">
                <Button variant="outline" on:click={unassignLptDevice} disabled={!$lptDevicePath} aria-label="Unassign Lpt serial device">&gt; Lpt</Button>
                <Button variant="outline" on:click={unassignBrakeTesterDevice} disabled={!$brakeTesterDevicePath} aria-label="Unassign BrakeTester serial device">&gt; BrakeTester</Button>
            </div>

            <div>
                <label for="serial-device-list" class="mb-1 block text-sm text-muted-foreground">Available serial devices</label>
                <select
                    id="serial-device-list"
                    size="8"
                    class="w-full rounded-md border border-border bg-muted p-2 text-sm text-foreground"
                    bind:value={$selectedSerialDevice}
                    on:change={(e) => selectSerialDevice(e.currentTarget.value)}
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
                <Button variant="default" on:click={runLptTest1} disabled={!$lptDevicePath}>Test 1</Button>
                <Button variant="secondary" on:click={runLptTest2} disabled={!$lptDevicePath}>Test 2</Button>
            </div>
        </div>
    </CardContent>
</Card>
