<script>
	import { DropdownMenu as DropdownMenuPrimitive } from "bits-ui";

	let { open = $bindable(), ...restProps } = $props();
</script>

<DropdownMenuPrimitive.Root bind:open {...restProps} />