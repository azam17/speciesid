@props(['active'])

@php
$classes = ($active ?? false)
            ? 'block w-full border-l-4 border-brand-600 bg-brand-50 py-2 pe-4 ps-3 text-start text-base font-bold text-brand-800 focus:outline-none focus:bg-brand-100 transition duration-150 ease-in-out'
            : 'block w-full border-l-4 border-transparent py-2 pe-4 ps-3 text-start text-base font-semibold text-slate-600 hover:border-slate-300 hover:bg-slate-50 hover:text-slate-950 focus:outline-none focus:bg-slate-50 transition duration-150 ease-in-out';
@endphp

<a {{ $attributes->merge(['class' => $classes]) }}>
    {{ $slot }}
</a>
