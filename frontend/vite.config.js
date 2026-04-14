import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';
import path from 'path';

export default defineConfig({

  server: {
    host: true
  },

  plugins: [
    tailwindcss(),
    svelte()
  ],
  build: {
    outDir: path.resolve(__dirname, "../www")
  },
  resolve: {
    alias: {
      $lib: path.resolve(__dirname, 'src/lib')
    }
  }
});