import defaultTheme from 'tailwindcss/defaultTheme';
import forms from '@tailwindcss/forms';

/** @type {import('tailwindcss').Config} */
export default {
    content: [
        './vendor/laravel/framework/src/Illuminate/Pagination/resources/views/*.blade.php',
        './storage/framework/views/*.php',
        './resources/views/**/*.blade.php',
    ],

    theme: {
        extend: {
            colors: {
                brand: {
                    50: '#edfdf7',
                    100: '#d5f7eb',
                    200: '#aeeed9',
                    300: '#79ddc0',
                    400: '#42c4a1',
                    500: '#22a987',
                    600: '#16886f',
                    700: '#146d5a',
                    800: '#135749',
                    900: '#11483d',
                    950: '#052b25',
                },
            },
            fontFamily: {
                sans: ['Figtree', ...defaultTheme.fontFamily.sans],
            },
        },
    },

    plugins: [forms],
};
