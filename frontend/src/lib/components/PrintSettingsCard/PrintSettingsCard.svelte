<script>
    import { onMount } from 'svelte';
    import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';
    import { printers, selectedPrinter, autoPrint, printStatus } from '$lib/stores/settings';
    import { selectPrinter, setAutoPrint, refreshPrinters } from '$lib/stores/settings';

    let selectedPrinterValue = '';

    function handlePrinterChange(event) {
        selectedPrinterValue = event.currentTarget.value;
        selectPrinter(selectedPrinterValue);
    }

    function handleAutoPrintChange(event) {
        setAutoPrint(event.currentTarget.checked);
    }

    onMount(() => {
        refreshPrinters();
    });

    $: selectedPrinterValue = $selectedPrinter || '';
</script>

<Card class="w-full">
    <CardHeader>
        <CardTitle class="text-base text-foreground">Print Settings</CardTitle>
    </CardHeader>

    <CardContent class="space-y-4 text-foreground">
        <div class="grid gap-2">
            <label for="printer-select" class="text-sm text-muted-foreground">Printer</label>
            <select
                id="printer-select"
                class="h-9 w-full min-w-0 max-w-full rounded-md border border-border bg-muted px-2 text-sm text-foreground"
                bind:value={selectedPrinterValue}
                onchange={handlePrinterChange}
            >
                <option value="">Unassigned</option>
                {#each $printers as printer}
                    <option value={printer.name}>{printer.name}{printer.info ? ` — ${printer.info}` : ''}</option>
                {/each}
            </select>
        </div>

        <div class="flex items-center justify-between gap-4 rounded-md border border-border bg-muted/60 p-3">
            <div>
                <div class="text-sm font-medium text-foreground">Auto print</div>
                <div class="text-xs text-muted-foreground">Automatically print generated PDFs when enabled.</div>
            </div>

            <label class="inline-flex cursor-pointer items-center gap-2">
                <input
                    type="checkbox"
                    class="h-5 w-5 rounded border-border bg-background"
                    checked={$autoPrint}
                    onchange={handleAutoPrintChange}
                />
                <span class="text-sm text-foreground">{$autoPrint ? 'On' : 'Off'}</span>
            </label>
        </div>

        <div class="text-xs text-muted-foreground">Current print status: {$printStatus}</div>
    </CardContent>
</Card>
