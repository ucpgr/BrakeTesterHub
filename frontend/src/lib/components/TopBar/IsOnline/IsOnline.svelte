<script>
    import { isOnline } from '$lib/stores/system';
    export let mode = 'FULL';
</script>

{#if mode === 'FULL'}
    <div class="w-32 flex items-center gap-2 font-medium">
    <span
            class="inline-block w-2.5 h-2.5 rounded-full"
            class:bg-green-500={$isOnline}
            class:bg-red-500={!$isOnline}
    ></span>
        <span class={$isOnline ? 'text-green-600' : 'text-red-600'}>
      {$isOnline ? 'Connected' : 'Disconnected'}
    </span>
    </div>
{:else}
    <div class="w-6 flex items-center justify-center">
    <span
            class="status-led"
            class:online={$isOnline}
            class:offline={!$isOnline}
    ></span>
    </div>
{/if}

<style>
    .status-led {
        width: 10px;
        height: 10px;
        border-radius: 9999px;
        animation: pulse 0.6s ease-in-out infinite alternate;
    }
    .online { background: #22c55e; }
    .offline { background: #ef4444; }

    @keyframes pulse {
        from { opacity: 0.4; }
        to   { opacity: 1; }
    }
</style>