<script>
  import { currentRoute } from '$lib/stores/router';
  import TopBar from '$lib/components/TopBar/TopBar.svelte';
  import TelemetryCard from '$lib/components/TelemetryCard/TelemetryCard.svelte';
  import AxleResultsCard from "$lib/components/AxleResultsCard/AxleResultsCard.svelte";
</script>

<TopBar></TopBar>
<div class="min-h-screen bg-background px-2 py-3 sm:px-4 sm:py-5 md:px-6 md:py-6 lg:px-8 lg:py-8">
  <div class="mx-auto w-full max-w-7xl px-1 py-2 sm:px-2 sm:py-3 md:px-4 md:py-4 lg:px-6 lg:py-6 space-y-6">
  {#if $currentRoute === '/live'}
    <h1>Live</h1>
  {:else if $currentRoute === '/settings'}
    <h1>Settings</h1>
  {:else if $currentRoute === '/history'}
    <h1>History</h1>
  {:else}
    <h1>Home</h1>
    <div class="grid md:grid-cols-[300px_1fr] gap-6">
      <TelemetryCard />
      <AxleResultsCard />
    </div>
  {/if}
  </div>
</div>
