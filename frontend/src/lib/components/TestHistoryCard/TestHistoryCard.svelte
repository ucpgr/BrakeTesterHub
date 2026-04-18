<script>
  import { onMount } from 'svelte';
  import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';

  const perPageOptions = Array.from({ length: 10 }, (_, index) => (index + 1) * 10);
  const thumbnailHeightPx = 100;
  const thumbnailWidthPx = Math.round(thumbnailHeightPx / Math.SQRT2);
  const detailColumnsClass =
    'grid-cols-[56px_1.55fr_.9fr_.9fr_.9fr_1.55fr_.9fr_.9fr_.9fr]';
  const minimumDetailRowsPerCard = 3;

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
  let detailsByTestId = {};

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

  function withDefault(value, fallback = '—') {
    return value === null || value === undefined || value === '' ? fallback : value;
  }

  function formatBrakeForce(row) {
    const left = withDefault(row?.leftBrakeForce);
    const right = withDefault(row?.rightBrakeForce);
    return `${left} / ${right}`;
  }

  function rowsForType(details, type) {
    const rows = details?.axleResults?.filter((row) => row.testType === type) ?? [];
    return [...rows].sort((a, b) => (Number(a.axleIndex) || 0) - (Number(b.axleIndex) || 0));
  }

  function detailRowsForTest(details) {
    const serviceRows = rowsForType(details, 'service');
    const handbrakeRows = rowsForType(details, 'hand_brake');
    const byAxle = new Map();

    for (const row of serviceRows) {
      byAxle.set(row.axleIndex, { axleIndex: row.axleIndex, service: row, handbrake: null });
    }

    for (const row of handbrakeRows) {
      if (!byAxle.has(row.axleIndex)) {
        byAxle.set(row.axleIndex, { axleIndex: row.axleIndex, service: null, handbrake: row });
      } else {
        byAxle.get(row.axleIndex).handbrake = row;
      }
    }

    const merged = Array.from(byAxle.values()).sort(
      (a, b) => (Number(a.axleIndex) || 0) - (Number(b.axleIndex) || 0)
    );
    while (merged.length < minimumDetailRowsPerCard) {
      merged.push({ axleIndex: '', service: null, handbrake: null });
    }

    return merged;
  }

  async function fetchDetailsForTests(historyTests) {
    if (!historyTests.length) {
      detailsByTestId = {};
      return;
    }

    const entries = await Promise.all(
      historyTests.map(async (test) => {
        try {
          const response = await fetch(`/api/history/${test.id}`);
          if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
          }
          const details = await response.json();
          return [test.id, details];
        } catch (error) {
          console.error(`Failed to fetch test details for test ${test.id}`, error);
          return [test.id, null];
        }
      })
    );

    detailsByTestId = Object.fromEntries(entries);
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

      await fetchDetailsForTests(tests);
    } catch (error) {
      console.error('Failed to fetch history', error);
      errorText = 'Failed to load history.';
      tests = [];
      totalCount = 0;
      detailsByTestId = {};
    } finally {
      loading = false;
    }
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

  function thumbnailHrefForTest(test) {
    const rawPath = test?.thumbnailFile;
    if (!rawPath) return '';
    const normalized = String(rawPath).replaceAll('\\', '/');
    if (normalized.startsWith('http://') || normalized.startsWith('https://')) {
      return normalized;
    }
    return normalized.startsWith('/') ? normalized : `/${normalized}`;
  }

  function preventEventBubble(event) {
    event.stopPropagation();
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
        <div class="hidden md:grid md:grid-cols-[270px_1fr]">
          <div></div>
          <div class="min-w-0">
            <div class="border-x border-t border-border/70 bg-muted/40" data-testid="history-inline-shared-header">
              <div class={`grid ${detailColumnsClass} border-b border-border/70 text-muted-foreground`}>
                <div class="border-r border-border/70 px-2 py-1"></div>
                <div class="col-span-4 border-r-2 border-border px-2 py-1 text-center text-[1.9rem] font-semibold leading-tight">Service</div>
                <div class="col-span-4 px-2 py-1 text-center text-[1.9rem] font-semibold leading-tight">Handbrake</div>
              </div>
              <div class={`grid ${detailColumnsClass} border-b border-border/70 text-xs font-semibold text-muted-foreground sm:text-sm`}>
                <div class="border-r border-border/70 px-2 py-1">Axle #</div>
                <div class="border-r border-border/70 px-2 py-1">Brake Force</div>
                <div class="border-r border-border/70 px-2 py-1">Efficiency</div>
                <div class="border-r border-border/70 px-2 py-1">Imbalance</div>
                <div class="border-r-2 border-border px-2 py-1">Weight</div>
                <div class="border-r border-border/70 px-2 py-1">Brake Force</div>
                <div class="border-r border-border/70 px-2 py-1">Efficiency</div>
                <div class="border-r border-border/70 px-2 py-1">Imbalance</div>
                <div class="px-2 py-1">Weight</div>
              </div>
            </div>
          </div>
        </div>
        {#each tests as test}
          <div class="grid w-full gap-4 rounded-md border border-border bg-background p-3 md:grid-cols-[270px_1fr]" data-testid={`history-row-${test.id}`}>
            <div class="flex items-start gap-3" data-testid={`history-meta-${test.id}`}>
              {#if thumbnailHrefForTest(test)}
                <a
                  href={thumbnailHrefForTest(test)}
                  target="_blank"
                  rel="noreferrer"
                  class="inline-block"
                  aria-label="Open thumbnail image"
                  on:click={preventEventBubble}
                >
                  <img
                    src={thumbnailHrefForTest(test)}
                    alt="Test report thumbnail"
                    class="shrink-0 rounded border border-border bg-white object-contain"
                    style={`height: ${thumbnailHeightPx}px; width: ${thumbnailWidthPx}px;`}
                    loading="lazy"
                  />
                </a>
              {:else}
                <div
                  class="shrink-0 rounded border border-border bg-white"
                  style={`height: ${thumbnailHeightPx}px; width: ${thumbnailWidthPx}px;`}
                  aria-label="No thumbnail available"
                ></div>
              {/if}

              <div class="min-w-0 space-y-2 text-left">
                <div class="text-sm font-semibold {test.vehicle?.reg ? 'text-foreground' : 'text-muted-foreground'}">{test.vehicle?.reg || 'NA'}</div>
                <div class="text-xs text-muted-foreground">{formatDateTime(test.createdAtUtc)}</div>
                {#if test.pdfFile && pdfHrefForTest(test)}
                  <a
                    class="inline-flex w-fit rounded border border-border px-2 py-1 text-xs text-foreground hover:bg-muted/40"
                    href={pdfHrefForTest(test)}
                    target="_blank"
                    rel="noreferrer"
                    data-testid={`history-pdf-${test.id}`}
                  >
                    Open PDF
                  </a>
                {:else}
                  <span class="inline-flex w-fit rounded border border-border px-2 py-1 text-xs text-muted-foreground">PDF unavailable</span>
                {/if}
                <div class="text-sm font-medium">
                  <span class="text-foreground">Result:</span>
                  <span class={statusClass(test.outcome)}> {statusLabel(test.outcome)}</span>
                </div>
              </div>
            </div>

            <div class="min-w-0 self-stretch" data-testid={`history-inline-details-${test.id}`}>
              <div class="grid h-full auto-rows-fr text-xs sm:text-sm">
                {#each detailRowsForTest(detailsByTestId[test.id]) as row}
                  <div class={`grid ${detailColumnsClass} h-full border-x border-b border-border/70`}>
                    <div class="border-r border-border/70 px-2 py-1 align-top font-medium">{withDefault(row.axleIndex, '')}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.service ? formatBrakeForce(row.service) : ''}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.service ? withDefault(row.service.efficiency, '') : ''}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.service ? withDefault(row.service.imbalance, '') : ''}</div>
                    <div class="border-r-2 border-border px-2 py-1 align-top">{row.service ? withDefault(row.service.weight, '') : ''}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.handbrake ? formatBrakeForce(row.handbrake) : ''}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.handbrake ? withDefault(row.handbrake.efficiency, '') : ''}</div>
                    <div class="border-r border-border/70 px-2 py-1 align-top">{row.handbrake ? withDefault(row.handbrake.imbalance, '') : ''}</div>
                    <div class="px-2 py-1 align-top">{row.handbrake ? withDefault(row.handbrake.weight, '') : ''}</div>
                  </div>
                {/each}
              </div>
            </div>
          </div>
        {/each}
      </div>

      <div class="flex items-center justify-between pt-1 text-sm">
        <button
          type="button"
          class="rounded border border-border px-3 py-1 disabled:opacity-50"
          on:click={() => {
            currentPage -= 1;
            loadHistory();
          }}
          disabled={currentPage <= 1}
        >
          Previous
        </button>
        <span class="text-muted-foreground">Page {currentPage} • {totalCount} total</span>
        <button
          type="button"
          class="rounded border border-border px-3 py-1 disabled:opacity-50"
          on:click={() => {
            currentPage += 1;
            loadHistory();
          }}
          disabled={currentPage * Number(selectedPerPage) >= totalCount}
        >
          Next
        </button>
      </div>
    {/if}
  </CardContent>
</Card>
