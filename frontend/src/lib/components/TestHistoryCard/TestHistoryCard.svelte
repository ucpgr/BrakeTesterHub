<script>
  import { onMount } from 'svelte';
  import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';

  const perPageOptions = Array.from({ length: 10 }, (_, index) => (index + 1) * 10);

  let loading = false;
  let errorText = '';

  let selectedYear = 'all';
  let selectedMonth = 'all';
  let selectedVehicle = 'all';
  let selectedPerPage = '20';
  let currentPage = 1;

  let tests = [];
  let filterYears = [];
  let filterMonths = [];
  let filterVehicles = [];
  let totalCount = 0;

  let modalOpen = false;
  let modalLoading = false;
  let selectedTestSummary = null;
  let selectedTestDetails = null;

  function formatDateTime(utcText) {
    if (!utcText) return 'Unknown date';
    const normalized = utcText.includes('T') ? utcText : utcText.replace(' ', 'T') + 'Z';
    const date = new Date(normalized);
    if (Number.isNaN(date.getTime())) return utcText;
    return date.toLocaleString();
  }

  function statusClass(outcome) {
    if (outcome === 'pass') return 'text-green-500';
    if (outcome === 'fail') return 'text-red-500';
    return 'text-muted-foreground';
  }

  function statusLabel(outcome) {
    if (outcome === 'pass') return 'Pass';
    if (outcome === 'fail') return 'Fail';
    return 'Unknown';
  }

  async function loadHistory() {
    loading = true;
    errorText = '';

    try {
      const params = new URLSearchParams();
      params.set('page', String(currentPage));
      params.set('perPage', selectedPerPage);

      if (selectedYear !== 'all') params.set('year', selectedYear);
      if (selectedMonth !== 'all') params.set('month', selectedMonth);
      if (selectedVehicle !== 'all') params.set('vehicle', selectedVehicle);

      const response = await fetch(`/api/history?${params.toString()}`);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const payload = await response.json();
      tests = Array.isArray(payload.tests) ? payload.tests : [];
      filterYears = Array.isArray(payload.filters?.years) ? payload.filters.years : [];
      filterMonths = Array.isArray(payload.filters?.months) ? payload.filters.months : [];
      filterVehicles = Array.isArray(payload.filters?.vehicles) ? payload.filters.vehicles : [];
      totalCount = Number(payload.totalCount ?? 0);
      selectedPerPage = String(payload.perPage ?? selectedPerPage);

      if (selectedYear !== 'all' && !filterYears.includes(Number(selectedYear))) {
        selectedYear = 'all';
      }
      if (selectedMonth !== 'all' && !filterMonths.some((month) => String(month.value) === selectedMonth)) {
        selectedMonth = 'all';
      }
      if (selectedVehicle !== 'all' && !filterVehicles.includes(selectedVehicle)) {
        selectedVehicle = 'all';
      }
    } catch (error) {
      console.error('Failed to fetch history', error);
      errorText = 'Failed to load history.';
      tests = [];
      totalCount = 0;
    } finally {
      loading = false;
    }
  }

  async function openDetails(test) {
    modalOpen = true;
    modalLoading = true;
    selectedTestSummary = test ?? null;
    selectedTestDetails = null;

    try {
      const response = await fetch(`/api/history/${test.id}`);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      selectedTestDetails = await response.json();
    } catch (error) {
      console.error('Failed to fetch test details', error);
      selectedTestDetails = null;
    } finally {
      modalLoading = false;
    }
  }

  function closeModal() {
    modalOpen = false;
    selectedTestSummary = null;
    selectedTestDetails = null;
  }

  function withDefault(value, fallback = '—') {
    return value === null || value === undefined || value === '' ? fallback : value;
  }

  function totalsForType(type) {
    const rows = selectedTestDetails?.axleResults?.filter((row) => row.testType === type) ?? [];

    const sum = (property) => rows.reduce((total, row) => total + (Number(row[property]) || 0), 0);

    return {
      leftBrakeForce: sum('leftBrakeForce'),
      rightBrakeForce: sum('rightBrakeForce'),
      efficiency: sum('efficiency'),
      imbalance: sum('imbalance'),
      weight: sum('weight')
    };
  }

  function resetPageAndLoad() {
    currentPage = 1;
    loadHistory();
  }

  function pdfHrefForTest(test) {
    const rawPath = test?.pdfFile;
    if (!rawPath) return '';
    const normalized = String(rawPath).replaceAll('\\', '/');
    if (normalized.startsWith('http://') || normalized.startsWith('https://')) {
      return normalized;
    }
    return normalized.startsWith('/') ? normalized : `/${normalized}`;
  }

  function handleModalBackdropKeydown(event) {
    if (event.key === 'Escape' || event.key === 'Enter' || event.key === ' ') {
      event.preventDefault();
      closeModal();
    }
  }

  function handleBackdropClick(event) {
    if (event.target === event.currentTarget) {
      closeModal();
    }
  }

  onMount(async () => {
    await loadHistory();
  });
</script>

<Card class="w-full">
  <CardHeader class="space-y-3">
    <div class="flex flex-wrap items-center gap-2 md:gap-3">
      <CardTitle class="text-base">Test History</CardTitle>
      <div class="flex flex-wrap items-center gap-2 text-sm">
        <label>
          Year
          <select class="ml-1 h-8 rounded-md border border-border bg-background px-2" bind:value={selectedYear} on:change={resetPageAndLoad}>
            <option value="all">All</option>
            {#each filterYears as year}
              <option value={String(year)}>{year}</option>
            {/each}
          </select>
        </label>

        <label>
          Month
          <select class="ml-1 h-8 rounded-md border border-border bg-background px-2" bind:value={selectedMonth} on:change={resetPageAndLoad}>
            <option value="all">All</option>
            {#each filterMonths as month}
              <option value={String(month.value)}>{month.label}</option>
            {/each}
          </select>
        </label>

        <label>
          Vehicle:
          <select class="ml-1 h-8 rounded-md border border-border bg-background px-2" bind:value={selectedVehicle} on:change={resetPageAndLoad}>
            <option value="all">All</option>
            {#each filterVehicles as reg}
              <option value={reg}>{reg}</option>
            {/each}
          </select>
        </label>

        <label>
          Results per page
          <select class="ml-1 h-8 rounded-md border border-border bg-background px-2" bind:value={selectedPerPage} on:change={resetPageAndLoad}>
            {#each perPageOptions as perPage}
              <option value={String(perPage)}>{perPage}</option>
            {/each}
          </select>
        </label>
      </div>
    </div>
  </CardHeader>

  <CardContent class="space-y-3">
    {#if loading}
      <div class="text-sm text-muted-foreground">Loading history…</div>
    {:else if errorText}
      <div class="text-sm text-red-500">{errorText}</div>
    {:else if tests.length === 0}
      <div class="text-sm text-muted-foreground">No tests found.</div>
    {:else}
      <div class="space-y-2">
        {#each tests as test}
          <button type="button" class="flex w-full items-center gap-3 rounded-md border border-border bg-background p-3 text-left hover:bg-muted/40" on:click={() => openDetails(test)}>
            <div class="h-[100px] w-[100px] shrink-0 rounded border border-border bg-white"></div>
            <div class="min-w-0 flex-1 space-y-1">
              <div class="text-sm font-semibold {test.vehicle?.reg ? 'text-foreground' : 'text-muted-foreground'}">{test.vehicle?.reg || 'NA'}</div>
              <div class="text-xs text-muted-foreground">{formatDateTime(test.createdAtUtc)}</div>
              <div class="text-sm font-medium {statusClass(test.outcome)}">{statusLabel(test.outcome)}</div>
            </div>
          </button>
        {/each}
      </div>

      <div class="flex items-center justify-between pt-1 text-sm">
        <button
          type="button"
          class="rounded border border-border px-3 py-1 disabled:opacity-50"
          on:click={() => { currentPage -= 1; loadHistory(); }}
          disabled={currentPage <= 1}
        >
          Previous
        </button>
        <span class="text-muted-foreground">Page {currentPage} • {totalCount} total</span>
        <button
          type="button"
          class="rounded border border-border px-3 py-1 disabled:opacity-50"
          on:click={() => { currentPage += 1; loadHistory(); }}
          disabled={(currentPage * Number(selectedPerPage)) >= totalCount}
        >
          Next
        </button>
      </div>
    {/if}
  </CardContent>
</Card>

{#if modalOpen}
  <div
    class="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4"
    role="button"
    tabindex="0"
    aria-label="Close modal backdrop"
    on:click={handleBackdropClick}
    on:keydown={handleModalBackdropKeydown}
  >
    <div class="max-h-[90vh] w-full max-w-5xl overflow-auto rounded-lg bg-background p-4" role="dialog" aria-modal="true">
      {#if modalLoading}
        <div class="text-sm text-muted-foreground">Loading test details…</div>
      {:else if !selectedTestDetails}
        <div class="space-y-4">
          <div class="text-sm text-red-500">Failed to load test details.</div>
        </div>
      {:else}
        <div class="mb-4">
          <h2 class="text-lg font-semibold">{selectedTestDetails.test?.vehicle?.reg || selectedTestSummary?.vehicle?.reg || 'NA'}</h2>
          <p class="text-sm text-muted-foreground">{formatDateTime(selectedTestDetails.test?.createdAtUtc || selectedTestSummary?.createdAtUtc)}</p>
        </div>

        <div class="overflow-x-auto">
          <table class="w-full border-collapse text-sm">
            <thead>
              <tr class="border-b border-border text-left">
                <th class="px-2 py-2">Type</th>
                <th class="px-2 py-2">L force</th>
                <th class="px-2 py-2">R force</th>
                <th class="px-2 py-2">Efficiency</th>
                <th class="px-2 py-2">Imbalance</th>
                <th class="px-2 py-2">Weight</th>
              </tr>
            </thead>
            <tbody>
              {#each selectedTestDetails.axleResults ?? [] as row}
                <tr class="border-b border-border/60">
                  <td class="px-2 py-2">{withDefault(row.testType)}</td>
                  <td class="px-2 py-2">{withDefault(row.leftBrakeForce)}</td>
                  <td class="px-2 py-2">{withDefault(row.rightBrakeForce)}</td>
                  <td class="px-2 py-2">{withDefault(row.efficiency)}</td>
                  <td class="px-2 py-2">{withDefault(row.imbalance)}</td>
                  <td class="px-2 py-2">{withDefault(row.weight)}</td>
                </tr>
              {/each}
              <tr class="bg-muted/40 font-medium">
                <td class="px-2 py-2">Service total</td>
                <td class="px-2 py-2">{totalsForType('service').leftBrakeForce}</td>
                <td class="px-2 py-2">{totalsForType('service').rightBrakeForce}</td>
                <td class="px-2 py-2">{totalsForType('service').efficiency}</td>
                <td class="px-2 py-2">{totalsForType('service').imbalance}</td>
                <td class="px-2 py-2">{totalsForType('service').weight}</td>
              </tr>

              <tr class="bg-muted/40 font-medium">
                <td class="px-2 py-2">Hand brake total</td>
                <td class="px-2 py-2">{totalsForType('hand_brake').leftBrakeForce}</td>
                <td class="px-2 py-2">{totalsForType('hand_brake').rightBrakeForce}</td>
                <td class="px-2 py-2">{totalsForType('hand_brake').efficiency}</td>
                <td class="px-2 py-2">{totalsForType('hand_brake').imbalance}</td>
                <td class="px-2 py-2">{totalsForType('hand_brake').weight}</td>
              </tr>
            </tbody>
          </table>
        </div>

      {/if}

      <div class="mt-4 flex items-center justify-between border-t border-border pt-3">
        {#if selectedTestSummary?.pdfFile && pdfHrefForTest(selectedTestSummary)}
          <a
            class="rounded border border-border px-3 py-1 text-sm text-foreground hover:bg-muted/40"
            href={pdfHrefForTest(selectedTestSummary)}
            target="_blank"
            rel="noreferrer"
          >
            Open PDF
          </a>
        {:else}
          <button type="button" class="rounded border border-border px-3 py-1 text-sm text-muted-foreground disabled:opacity-60" disabled>Open PDF</button>
        {/if}
        <button type="button" class="rounded border border-border px-3 py-1 text-foreground hover:bg-muted/40" on:click={closeModal}>Close</button>
      </div>
    </div>
  </div>
{/if}
