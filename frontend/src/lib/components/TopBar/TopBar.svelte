<script>
    import { onMount, onDestroy } from 'svelte';
    import IsOnline from './IsOnline/IsOnline.svelte';
    import Status from './Status/Status.svelte';
    import Nav from './Nav/Nav.svelte';

    let width = window.innerWidth;

    function onResize() {
        width = window.innerWidth;
    }

    onMount(() => {
        window.addEventListener('resize', onResize);
    });

    onDestroy(() => {
        window.removeEventListener('resize', onResize);
    });

    $: mode =
        width >= 1100 ? 'FULL'
            : width >= 900 ? 'LED'
                : width >= 700 ? 'SHORT'
                    : 'MIN';
</script>

<header class="w-full border-b bg-background px-4 py-2">
    <div class="flex w-full items-center gap-4">

        <!-- Online: fixed -->
        <div class="shrink-0">
            <IsOnline {mode} />
        </div>

        <!-- Title: fixed (when shown) -->
        {#if mode === 'FULL'}
            <div class="shrink-0 whitespace-nowrap font-semibold text-foreground">
                Brake Tester Hub
            </div>
        {:else if mode === 'LED' || mode === 'SHORT'}
            <div class="shrink-0 whitespace-nowrap font-semibold text-foreground">
                Brake Test
            </div>
        {/if}

        <!-- STATUS: THE ONLY FLEXIBLE ITEM -->
        <div class="flex-1 min-w-0">
            <Status />
        </div>

        <!-- NAV: fixed, never absolute -->
        <div class="shrink-0">
            <Nav mode={mode === 'MIN' ? 'menu' : 'inline'} />
        </div>

    </div>
</header>