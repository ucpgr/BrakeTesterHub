import { Activity, Settings, History } from 'lucide-svelte';

export const routes = [
    {
        path: '/',
        label: 'Live',
        icon: Activity,
        nav: true
    },
    {
        path: '/settings',
        label: 'Settings',
        icon: Settings,
        nav: true
    },
    {
        path: '/history',
        label: 'History',
        icon: History,
        nav: true
    }
];