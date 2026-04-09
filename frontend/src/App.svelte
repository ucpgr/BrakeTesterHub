<script>
  import { currentRoute } from '$lib/stores/router';
  import TopBar from '$lib/components/TopBar/TopBar.svelte';
  import TelemetryCard from '$lib/components/TelemetryCard/TelemetryCard.svelte';
  import AxleResultsCard from "$lib/components/AxleResultsCard/AxleResultsCard.svelte";
</script>

<TopBar></TopBar>
<div class="min-h-screen bg-background p-8">
  <div class="mx-auto w-full max-w-7xl px-6 py-6 space-y-6">
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

