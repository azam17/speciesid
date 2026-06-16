<!DOCTYPE html>
<html lang="{{ str_replace('_', '-', app()->getLocale()) }}">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <meta name="csrf-token" content="{{ csrf_token() }}">
        <title>@yield('title', config('app.name', 'SpeciesID'))</title>
        <link rel="preconnect" href="https://fonts.bunny.net">
        <link href="https://fonts.bunny.net/css?family=figtree:400,500,600,700,800&display=swap" rel="stylesheet" />
        @vite(['resources/css/app.css', 'resources/js/app.js'])
    </head>
    <body class="bg-[#f5f7f4] font-sans text-slate-950 antialiased">
        <main class="min-h-screen">
            <div class="grid min-h-screen lg:grid-cols-[minmax(420px,0.9fr)_1.1fr]">
                <section class="hidden border-r border-slate-900/10 bg-slate-950 p-10 text-white lg:flex lg:flex-col lg:justify-between">
                    <a href="{{ route('welcome') }}" class="flex items-center">
                        <x-application-logo class="h-10 w-auto [&_text]:fill-white" />
                    </a>

                    <div>
                        <p class="text-sm font-bold uppercase tracking-widest text-brand-200">Halal lab workspace</p>
                        <h1 class="mt-4 max-w-xl text-5xl font-extrabold leading-tight tracking-normal">Check meat samples and see what needs attention.</h1>
                        <p class="mt-6 max-w-lg text-base leading-7 text-slate-300">The sample is the meat product. SpeciesID reads the sequencing files prepared from that sample, then shows CLEAR, ALERT, or REVIEW.</p>
                    </div>

                    <div class="grid grid-cols-3 gap-px overflow-hidden rounded-2xl border border-white/10 bg-white/10 text-sm">
                        <div class="bg-slate-950 p-4">
                            <p class="text-slate-400">Use</p>
                            <p class="mt-1 font-extrabold">Screening</p>
                        </div>
                        <div class="bg-slate-950 p-4">
                            <p class="text-slate-400">Reports</p>
                            <p class="mt-1 font-extrabold">Plain</p>
                        </div>
                        <div class="bg-slate-950 p-4">
                            <p class="text-slate-400">Follow-up</p>
                            <p class="mt-1 font-extrabold">Guided</p>
                        </div>
                    </div>
                </section>

                <section class="flex items-center justify-center px-4 py-8 sm:px-6 lg:px-12">
                    <div class="w-full max-w-[520px]">
                        <div class="mb-8 flex items-center justify-between lg:hidden">
                            <a href="{{ route('welcome') }}">
                                <x-application-logo class="h-10 w-auto" />
                            </a>
                            <a href="{{ route('welcome') }}" class="rounded-full border border-slate-900/10 bg-white px-4 py-2 text-sm font-bold text-slate-700 shadow-sm hover:text-slate-950">Home</a>
                        </div>

                        <div class="border-y border-slate-900/10 bg-white px-5 py-7 shadow-sm sm:rounded-[28px] sm:border sm:p-8">
                            {{ $slot }}
                        </div>
                    </div>
                </section>
            </div>
        </main>
    </body>
</html>
