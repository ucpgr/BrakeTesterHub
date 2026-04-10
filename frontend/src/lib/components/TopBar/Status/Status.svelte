<script>
    import { status } from '$lib/stores/system';
    import { Info, AlertTriangle, XCircle, Loader2, PauseCircle } from 'lucide-svelte';
    import { fade } from 'svelte/transition';
    import { cubicOut } from 'svelte/easing';

    const styles = {
        idle: { icon: PauseCircle, class: 'text-muted-foreground' },
        info: { icon: Info, class: 'text-blue-600 dark:text-blue-400' },
        warning: { icon: AlertTriangle, class: 'text-yellow-600 dark:text-yellow-400' },
        error: { icon: XCircle, class: 'text-red-600 dark:text-red-400' },
        progress: { icon: Loader2, class: 'text-muted-foreground' }
    };
</script>

<div
    title={$status.text}
    class="flex w-full min-w-0 items-center gap-2 overflow-hidden text-left text-sm"
>
    <svelte:component
        this={styles[$status.level].icon}
        class={`h-4 w-4 shrink-0 ${styles[$status.level].class} ${
            $status.level === 'progress' ? 'animate-spin' : ''
        }`}
    />

    <span
        class={`min-w-0 flex-1 truncate ${styles[$status.level].class}`}
        transition:fade={{ duration: 160, easing: cubicOut }}
    >
        {$status.text}
    </span>
</div>
