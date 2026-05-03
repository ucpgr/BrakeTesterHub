<script>
    import { telemetry, peaks, efficiencies, imbalance } from '$lib/stores/telemetry';

    import {Card, CardContent, CardHeader, CardTitle} from '$lib/components/ui/card';
    import { Badge } from '$lib/components/ui/badge';

    import { Circle, Check, X } from 'lucide-svelte';

    const MAX_FORCE = 600;
    const clamp = (v) => Math.max(0, Math.min(1, v));

    const rollerClass = (active) =>
        `h-3 w-3 ${active ? 'text-green-500' : 'text-muted-foreground'}`;
</script>

<Card class="w-full max-w-md mx-auto">
    <CardHeader>
        <CardTitle class="text-base">Telemetry</CardTitle>
    </CardHeader>

    <CardContent class="p-4 space-y-4">

        <!-- STATE ROW -->
        <div class="flex justify-between text-sm">
            <div class="flex items-center gap-2">
                <Circle
                        class={rollerClass($telemetry.leftRollerActive)}
                        fill="currentColor"
                />

                {#if $telemetry.leftWheelDetected}
                    <Check class="h-4 w-4 text-green-500" />
                {:else}
                    <X class="h-4 w-4 text-red-500" />
                {/if}

                <span class="text-muted-foreground">Left</span>
            </div>

            <div class="flex items-center gap-2">
                <Circle
                        class={rollerClass($telemetry.rightRollerActive)}
                        fill="currentColor"
                />

                {#if $telemetry.rightWheelDetected}
                    <Check class="h-4 w-4 text-green-500" />
                {:else}
                    <X class="h-4 w-4 text-red-500" />
                {/if}

                <span class="text-muted-foreground">Right</span>
            </div>
        </div>

        <!-- FORCE BARS + WEIGHT -->
        <div class="grid grid-cols-[1fr_auto_1fr] items-end gap-4">

            <!-- LEFT -->
            <div class="flex flex-col items-center gap-1">
                <div class="relative h-36 w-7 rounded bg-muted overflow-hidden">
                    <div
                            class="absolute bottom-0 w-full bg-blue-500 transition-[height] duration-150"
                            style="height:{clamp($telemetry.leftForce / MAX_FORCE) * 100}%"
                    ></div>
                    <div
                            class="absolute w-full h-0.5 bg-foreground"
                            style="bottom:{clamp($peaks.left / MAX_FORCE) * 100}%"
                    ></div>
                </div>

                <div class="font-semibold tabular-nums">
                    {$telemetry.leftForce.toFixed(0)} kgf
                </div>

                <div class="text-xs text-muted-foreground">
                    peak {$peaks.left.toFixed(0)}
                </div>

                <Badge variant="secondary">
                    {$efficiencies.left ?? '—'}%
                </Badge>
            </div>

            <!-- WEIGHT -->
            <div class="flex flex-col items-center justify-end pb-2">
                <span class="text-xs text-muted-foreground">AXLE WEIGHT</span>
                <span class="text-lg font-semibold tabular-nums">
          {$telemetry.axleWeight.toFixed(0)} kg
        </span>
            </div>

            <!-- RIGHT -->
            <div class="flex flex-col items-center gap-1">
                <div class="relative h-36 w-7 rounded bg-muted overflow-hidden">
                    <div
                            class="absolute bottom-0 w-full bg-orange-500 transition-[height] duration-150"
                            style="height:{clamp($telemetry.rightForce / MAX_FORCE) * 100}%"
                    ></div>
                    <div
                            class="absolute w-full h-0.5 bg-foreground"
                            style="bottom:{clamp($peaks.right / MAX_FORCE) * 100}%"
                    ></div>
                </div>

                <div class="font-semibold tabular-nums">
                    {$telemetry.rightForce.toFixed(0)} kgf
                </div>

                <div class="text-xs text-muted-foreground">
                    peak {$peaks.right.toFixed(0)}
                </div>

                <Badge variant="secondary">
                    {$efficiencies.right ?? '—'}%
                </Badge>
            </div>
        </div>

        <!-- IMBALANCE -->
        <div class="space-y-1">
            <div class="text-xs text-center text-muted-foreground">
                IMBALANCE
            </div>

            <div class="relative h-2 rounded bg-muted">
                <div class="absolute left-1/2 top-0 h-full w-0.5 bg-muted-foreground/60"></div>
                <div
                        class="absolute -top-2 h-6 w-2 rounded bg-yellow-400"
                        style="left:{clamp(($imbalance + 100) / 200) * 100}%"
                ></div>
            </div>

            <div class="text-center text-sm tabular-nums">
                {$imbalance.toFixed(1)}%
                {#if $imbalance > 0}
                    <span class="text-muted-foreground">(R weaker)</span>
                {:else if $imbalance < 0}
                    <span class="text-muted-foreground">(L weaker)</span>
                {/if}
            </div>
        </div>

    </CardContent>
</Card>