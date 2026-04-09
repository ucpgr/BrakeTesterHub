<script>
    import { setStatus, status } from '$lib/stores/system';

    let text = '';
    let level = 'info';

    $: if ($status) {
        text = $status.text;
        level = $status.level;
    }

    function apply() {
        setStatus({ level, text });
    }
</script>

<div class="max-w-md space-y-4 rounded-md border bg-card p-4">
    <h3 class="text-sm font-semibold text-foreground">
        Status Tester
    </h3>

    <!-- Text input -->
    <div class="space-y-1">
        <label for="status-text" class="text-xs text-muted-foreground">
            Status text
        </label>
        <input
                id="status-text"
                type="text"
                bind:value={text}
                class="w-full rounded-md border bg-background px-3 py-2 text-sm
             focus:outline-none focus:ring-2 focus:ring-ring"
                placeholder="Enter status message…"
        />
    </div>

    <!-- Level dropdown -->
    <div class="space-y-1">
        <label for="status-level" class="text-xs text-muted-foreground">
            Status level
        </label>
        <select
                id="status-level"
                bind:value={level}
                class="w-full rounded-md border bg-background px-3 py-2 text-sm
             focus:outline-none focus:ring-2 focus:ring-ring"
        >
            <option value="idle">Idle</option>
            <option value="info">Info</option>
            <option value="warning">Warning</option>
            <option value="error">Error</option>
            <option value="progress">Progress</option>
        </select>
    </div>

    <button
            on:click={apply}
            class="inline-flex items-center justify-center rounded-md
           bg-primary px-4 py-2 text-sm font-medium text-primary-foreground
           hover:bg-primary/90 focus:outline-none focus:ring-2
           focus:ring-ring"
    >
        Set status
    </button>

    <p class="text-xs text-muted-foreground">
        Status will automatically revert to <strong>Idle</strong> after 5 seconds.
    </p>
</div>