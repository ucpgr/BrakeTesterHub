<script>
    import { routes } from '$lib/routes';
    import { currentRoute, navigate } from '$lib/stores/router';
    import { Menu } from 'lucide-svelte';

    export let mode = 'inline'; // 'inline' | 'menu'

    const baseBtn =
        'inline-flex items-center gap-2 rounded-md px-3 py-2 text-sm transition-colors ' +
        'focus:outline-none focus:ring-2 focus:ring-ring focus:ring-offset-2';

    const inactive = 'text-foreground hover:bg-muted';
    const active = 'bg-muted text-foreground cursor-default';

    const iconClass = 'w-4 h-4';

    function isActive(path, current) {
        return path === current;
    }
</script>

{#if mode === 'inline'}
    <nav class="flex items-center gap-1 whitespace-nowrap">
        {#each routes.filter(r => r.nav) as route}
            <button
                class={`${baseBtn} ${isActive(route.path, $currentRoute) ? active : inactive}`}
                disabled={isActive(route.path, $currentRoute)}
                aria-current={isActive(route.path, $currentRoute) ? 'page' : undefined}
                on:click={() => navigate(route.path)}
            >
                <svelte:component
                    this={route.icon}
                    class={`${iconClass} ${
                        isActive(route.path, $currentRoute) ? '' : 'text-muted-foreground'
                    }`}
                />
                {route.label}
            </button>
        {/each}
    </nav>
{:else}
    <details class="relative">
        <summary
            class="inline-flex list-none cursor-pointer items-center justify-center rounded-md p-2 hover:bg-muted focus:outline-none"
            aria-label="Navigation menu"
        >
            <Menu class="w-5 h-5 text-foreground" />
        </summary>
        <nav class="absolute right-0 z-50 mt-2 min-w-[180px] rounded-md border bg-background p-1 shadow-md">
            {#each routes.filter(r => r.nav) as route}
                <button
                    class={`flex w-full items-center gap-2 rounded-sm px-2 py-2 text-left text-sm ${
                        isActive(route.path, $currentRoute) ? 'bg-muted text-foreground' : 'hover:bg-muted'
                    }`}
                    disabled={isActive(route.path, $currentRoute)}
                    on:click={() => navigate(route.path)}
                >
                    <svelte:component
                        this={route.icon}
                        class={`w-4 h-4 ${
                            isActive(route.path, $currentRoute) ? '' : 'text-muted-foreground'
                        }`}
                    />
                    {route.label}
                </button>
            {/each}
        </nav>
    </details>
{/if}
