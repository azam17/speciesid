<!DOCTYPE html>
<html lang="{{ str_replace('_', '-', app()->getLocale()) }}">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Features | {{ config('app.name', 'SpeciesID') }}</title>
        <link rel="preconnect" href="https://fonts.bunny.net">
        <link href="https://fonts.bunny.net/css?family=figtree:400,500,600,700,800&display=swap" rel="stylesheet" />
        @vite(['resources/css/app.css', 'resources/js/app.js'])
    </head>
    <body class="bg-[#f7f8f4] font-sans text-slate-950 antialiased">
        <header class="sticky top-0 z-30 border-b border-slate-900/10 bg-[#f7f8f4]/92 backdrop-blur">
            <div class="mx-auto flex h-16 max-w-[1440px] items-center justify-between px-4 sm:px-6 lg:px-8">
                <a href="{{ route('welcome') }}" class="flex items-center">
                    <x-application-logo class="h-9 w-auto" />
                </a>
                <nav class="hidden items-center rounded-full border border-slate-900/10 bg-white px-2 py-1 text-sm font-bold text-slate-600 shadow-sm md:flex">
                    <a class="rounded-full px-4 py-2 hover:bg-slate-950 hover:text-white" href="{{ route('welcome') }}#how">How it works</a>
                    <a class="rounded-full bg-slate-950 px-4 py-2 text-white" href="{{ route('features') }}">Features</a>
                    <a class="rounded-full px-4 py-2 hover:bg-slate-950 hover:text-white" href="{{ route('welcome') }}#reports">Reports</a>
                </nav>
                <div class="flex items-center gap-2">
                    @auth
                        <a href="{{ route('dashboard') }}" class="rounded-full border border-slate-900/10 bg-white px-4 py-2 text-sm font-bold text-slate-900 shadow-sm hover:border-brand-400">Dashboard</a>
                    @else
                        <a href="{{ route('login') }}" class="rounded-full px-4 py-2 text-sm font-bold text-slate-700 hover:text-slate-950">Log in</a>
                    @endauth
                </div>
            </div>
        </header>

        <main>
            <section class="mx-auto max-w-[1440px] px-4 py-14 sm:px-6 lg:px-8">
                <div class="max-w-4xl">
                    <p class="text-sm font-extrabold uppercase tracking-widest text-brand-700">Features</p>
                    <h1 class="mt-4 text-5xl font-extrabold leading-tight tracking-normal text-slate-950 sm:text-6xl">
                        SpeciesID helps labs screen faster. Standard methods remain important for confirmation.
                    </h1>
                    <p class="mt-6 text-lg leading-8 text-slate-600">
                        Use SpeciesID to quickly sort processed meat samples into clear, alert, and review groups after the lab has prepared sequencing files from the meat. Use standard confirmatory tests when the lab needs a targeted final check.
                    </p>
                </div>
            </section>

            <section class="mx-auto max-w-[1440px] px-4 pb-16 sm:px-6 lg:px-8">
                <div class="grid gap-5 lg:grid-cols-2">
                    <article class="rounded-[32px] border border-brand-200 bg-white p-6 shadow-sm">
                        <div class="flex items-start justify-between gap-4">
                            <div>
                                <p class="text-sm font-extrabold uppercase tracking-widest text-brand-700">Our product</p>
                                <h2 class="mt-2 text-3xl font-extrabold text-slate-950">SpeciesID screening</h2>
                            </div>
                            <span class="rounded-full bg-brand-50 px-3 py-1 text-xs font-extrabold text-brand-800 ring-1 ring-brand-200">Fast triage</span>
                        </div>

                        <div class="mt-8 divide-y divide-slate-200 border-y border-slate-200">
                            @foreach ([
                                ['Purpose', 'Quickly screen many samples and find which ones need attention.'],
                                ['Best for', 'Processed meat, mixed meat products, routine batch checks, and early warning.'],
                                ['Result style', 'CLEAR, ALERT, or REVIEW with short notes and next-step guidance.'],
                                ['Controls', 'Shows whether control samples suggest the batch needs review.'],
                                ['Reports', 'Exports PDF, JSON, TSV, and ZIP evidence package for lab records.'],
                                ['Position', 'Screening support, not a religious or legal certification.'],
                            ] as [$label, $body])
                                <div class="grid gap-3 py-5 sm:grid-cols-[160px_1fr]">
                                    <p class="font-extrabold text-slate-950">{{ $label }}</p>
                                    <p class="leading-6 text-slate-600">{{ $body }}</p>
                                </div>
                            @endforeach
                        </div>
                    </article>

                    <article class="rounded-[32px] border border-slate-900/10 bg-slate-950 p-6 text-white shadow-sm">
                        <div class="flex items-start justify-between gap-4">
                            <div>
                                <p class="text-sm font-extrabold uppercase tracking-widest text-brand-200">Standard method</p>
                                <h2 class="mt-2 text-3xl font-extrabold">Targeted lab confirmation</h2>
                            </div>
                            <span class="rounded-full bg-white/10 px-3 py-1 text-xs font-extrabold text-white ring-1 ring-white/15">Final check</span>
                        </div>

                        <div class="mt-8 divide-y divide-white/10 border-y border-white/10">
                            @foreach ([
                                ['Purpose', 'Confirm a specific concern using an established targeted test.'],
                                ['Best for', 'Final follow-up when a sample is flagged or when a specific species must be checked.'],
                                ['Result style', 'Usually focused on one or a few selected species at a time.'],
                                ['Controls', 'Handled inside the lab method and documented in the final result.'],
                                ['Reports', 'Usually issued according to the lab quality system and authority requirements.'],
                                ['Position', 'Still needed when a formal confirmatory result is required.'],
                            ] as [$label, $body])
                                <div class="grid gap-3 py-5 sm:grid-cols-[160px_1fr]">
                                    <p class="font-extrabold text-white">{{ $label }}</p>
                                    <p class="leading-6 text-slate-300">{{ $body }}</p>
                                </div>
                            @endforeach
                        </div>
                    </article>
                </div>
            </section>

            <section class="border-y border-slate-900/10 bg-white">
                <div class="mx-auto grid max-w-[1440px] gap-8 px-4 py-16 sm:px-6 lg:grid-cols-[0.65fr_1fr] lg:px-8">
                    <div>
                        <p class="text-sm font-extrabold uppercase tracking-widest text-brand-700">When to use each</p>
                        <h2 class="mt-3 text-4xl font-extrabold leading-tight tracking-normal">Screen first. Confirm when needed.</h2>
                    </div>
                    <div class="grid gap-4 md:grid-cols-3">
                        @foreach ([
                            ['Routine check', 'Use SpeciesID to screen batches quickly.'],
                            ['Unexpected signal', 'Use SpeciesID to identify samples that need review.'],
                            ['Final decision', 'Use standard confirmatory testing when a formal result is required.'],
                        ] as [$title, $body])
                            <div class="rounded-3xl border border-slate-900/10 bg-[#f7f8f4] p-5">
                                <h3 class="font-extrabold text-slate-950">{{ $title }}</h3>
                                <p class="mt-2 text-sm leading-6 text-slate-600">{{ $body }}</p>
                            </div>
                        @endforeach
                    </div>
                </div>
            </section>
        </main>
    </body>
</html>
