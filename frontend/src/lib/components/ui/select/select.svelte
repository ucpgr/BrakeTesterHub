<script>
	import { Select as SelectPrimitive } from "bits-ui";

	let {
		open = $bindable(),
		value = $bindable(),
		...restProps
	} = $props();
</script>

<SelectPrimitive.Root bind:open bind:value={value} {...restProps} />