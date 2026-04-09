import { writable } from 'svelte/store';

export const currentRoute = writable(window.location.pathname);

export function navigate(path) {
    if (path === window.location.pathname) return;

    history.pushState({}, '', path);
    currentRoute.set(path);
}

window.addEventListener('popstate', () => {
    currentRoute.set(window.location.pathname);
});