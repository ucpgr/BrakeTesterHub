<script>
    import { axleResults } from '$lib/stores/results';
    import { Card, CardHeader, CardTitle, CardContent } from '$lib/components/ui/card';
    import { Badge } from '$lib/components/ui/badge';
</script>

<Card class="w-full">
    <CardHeader>
        <CardTitle class="text-base">Test Results</CardTitle>
    </CardHeader>

    <CardContent>

        <!-- Header Row -->
        <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] text-xs text-muted-foreground pb-2 border-b">
            <div>Axle</div>
            <div class="text-right">Left</div>
            <div class="text-right">Right</div>
            <div class="text-right">Weight</div>
            <div class="text-right">Imbal</div>
            <div class="text-center">Status</div>
        </div>

        {#each $axleResults as axle}
            <div class="py-3 border-b last:border-0">

                <!-- Main Row -->
                <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] items-center">

                    <div class="font-medium">
                        {axle.axle}
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.leftForce} kgf
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.rightForce} kgf
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.weight} kg
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.imbalance.toFixed(1)}%
                    </div>

                    <div class="text-center">
                        <Badge variant={axle.status === 'PASS' ? 'default' : 'destructive'}>
                            {axle.status}
                        </Badge>
                    </div>

                </div>

                <!-- Efficiency Row -->
                <div class="grid grid-cols-[50px_1fr_1fr_1fr_1fr_90px] text-xs text-muted-foreground mt-1">

                    <div></div>

                    <div class="text-right tabular-nums">
                        {axle.leftEff}%
                    </div>

                    <div class="text-right tabular-nums">
                        {axle.rightEff}%
                    </div>

                    <div></div>
                    <div></div>
                    <div></div>

                </div>

            </div>
        {/each}

    </CardContent>
</Card>