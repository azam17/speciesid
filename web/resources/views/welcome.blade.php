<!DOCTYPE html>
<html lang="{{ str_replace('_', '-', app()->getLocale()) }}">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>{{ config('app.name', 'SpeciesID') }}</title>
        <link rel="preconnect" href="https://fonts.bunny.net">
        <link href="https://fonts.bunny.net/css?family=figtree:400,500,600,700,800&display=swap" rel="stylesheet" />
        @vite(['resources/css/app.css', 'resources/js/app.js'])
    </head>
    <body class="bg-[#f7f8f4] font-sans text-slate-950 antialiased">
        <div class="min-h-screen">
            <header class="sticky top-0 z-30 border-b border-slate-900/10 bg-[#f7f8f4]/92 backdrop-blur">
                <div class="mx-auto flex h-16 max-w-[1440px] items-center justify-between px-4 sm:px-6 lg:px-8">
                    <a href="{{ route('welcome') }}" class="flex items-center">
                        <x-application-logo class="h-9 w-auto" />
                    </a>
                    <nav class="hidden items-center rounded-full border border-slate-900/10 bg-white px-2 py-1 text-sm font-bold text-slate-600 shadow-sm md:flex">
                        <a class="rounded-full px-4 py-2 hover:bg-slate-950 hover:text-white" href="#how">How it works</a>
                        <a class="rounded-full px-4 py-2 hover:bg-slate-950 hover:text-white" href="{{ route('features') }}">Features</a>
                        <a class="rounded-full px-4 py-2 hover:bg-slate-950 hover:text-white" href="#reports">Reports</a>
                    </nav>
                    <div class="flex items-center gap-2">
                        @auth
                            <a href="{{ route('dashboard') }}" class="rounded-full border border-slate-900/10 bg-white px-4 py-2 text-sm font-bold text-slate-900 shadow-sm hover:border-brand-400">Dashboard</a>
                        @else
                            <a href="{{ route('login') }}" class="rounded-full px-4 py-2 text-sm font-bold text-slate-700 hover:text-slate-950">Log in</a>
                            @if (Route::has('register'))
                                <a href="{{ route('register') }}" class="hidden rounded-full bg-slate-950 px-5 py-2.5 text-sm font-bold text-white shadow-sm hover:bg-brand-800 sm:inline-flex">Create account</a>
                            @endif
                        @endauth
                    </div>
                </div>
            </header>

            <main>
                <section class="mx-auto grid max-w-[1440px] gap-10 px-4 pb-14 pt-10 sm:px-6 lg:grid-cols-[0.9fr_1.1fr] lg:px-8 lg:pt-16">
                    <div class="flex flex-col justify-center">
                        <div class="inline-flex w-fit rounded-full border border-brand-300 bg-white px-3 py-1 text-xs font-extrabold uppercase tracking-widest text-brand-800">
                            Halal meat lab screening
                        </div>
                        <h1 class="mt-7 max-w-3xl text-5xl font-extrabold leading-[1.02] tracking-normal text-slate-950 sm:text-6xl lg:text-7xl">
                            Check meat samples for unexpected species, faster.
                        </h1>
                        <p class="mt-6 max-w-2xl text-lg leading-8 text-slate-600">
                            SpeciesID helps halal authentication labs screen processed meat samples. The lab prepares sequencing files from the meat, then SpeciesID shows whether the result is CLEAR, needs REVIEW, or shows an ALERT.
                        </p>
                        <div class="mt-8 flex flex-col gap-3 sm:flex-row">
                            <a href="{{ auth()->check() ? route('upload.create') : route('login') }}" class="inline-flex items-center justify-center rounded-full bg-brand-700 px-6 py-3 text-sm font-bold text-white shadow-sm hover:bg-brand-800">
                                Start a sample check
                            </a>
                            <a href="{{ route('features') }}" class="inline-flex items-center justify-center rounded-full border border-slate-900/10 bg-white px-6 py-3 text-sm font-bold text-slate-900 shadow-sm hover:border-brand-400">
                                Compare methods
                            </a>
                        </div>
                        <div class="mt-10 grid max-w-2xl gap-px overflow-hidden rounded-3xl border border-slate-900/10 bg-slate-900/10 text-sm sm:grid-cols-3">
                            <div class="bg-white p-4">
                                <p class="font-extrabold text-emerald-700">CLEAR</p>
                                <p class="mt-1 text-slate-600">No concern found in the screened panel.</p>
                            </div>
                            <div class="bg-white p-4">
                                <p class="font-extrabold text-amber-700">REVIEW</p>
                                <p class="mt-1 text-slate-600">Result needs human review or another test.</p>
                            </div>
                            <div class="bg-white p-4">
                                <p class="font-extrabold text-rose-700">ALERT</p>
                                <p class="mt-1 text-slate-600">Unexpected species signal detected.</p>
                            </div>
                        </div>
                    </div>

                    <div class="relative">
                        <div class="relative overflow-hidden rounded-[32px] border border-slate-900/10 bg-slate-950 p-3 shadow-2xl shadow-slate-900/20">
                            <div class="rounded-[24px] bg-white p-5">
                                <div class="flex flex-col gap-4 border-b border-slate-200 pb-5 sm:flex-row sm:items-center sm:justify-between">
                                    <div>
                                        <p class="text-xs font-extrabold uppercase tracking-widest text-brand-700">Sample result</p>
                                        <h2 class="mt-1 text-2xl font-extrabold text-slate-950">Burger patty batch</h2>
                                    </div>
                                    <span class="w-fit rounded-full bg-rose-50 px-4 py-2 text-sm font-extrabold text-rose-700 ring-1 ring-rose-200">ALERT</span>
                                </div>

                                <div class="mt-5 grid gap-3 sm:grid-cols-3">
                                    <div class="rounded-2xl bg-slate-50 p-4">
                                        <p class="text-sm text-slate-500">Sample type</p>
                                        <p class="mt-2 text-xl font-extrabold">Processed meat</p>
                                    </div>
                                    <div class="rounded-2xl bg-slate-50 p-4">
                                        <p class="text-sm text-slate-500">Main concern</p>
                                        <p class="mt-2 text-xl font-extrabold">Unexpected species</p>
                                    </div>
                                    <div class="rounded-2xl bg-slate-50 p-4">
                                        <p class="text-sm text-slate-500">Next step</p>
                                        <p class="mt-2 text-xl font-extrabold">Confirm</p>
                                    </div>
                                </div>

                                <div class="mt-5 overflow-hidden rounded-2xl border border-slate-200">
                                    <div class="grid grid-cols-[1fr_0.8fr_0.9fr] bg-slate-50 px-4 py-3 text-xs font-extrabold uppercase tracking-widest text-slate-500">
                                        <span>Finding</span><span>Status</span><span>Lab action</span>
                                    </div>
                                    <div class="divide-y divide-slate-100 text-sm">
                                        <div class="grid grid-cols-[1fr_0.8fr_0.9fr] px-4 py-4">
                                            <span class="font-bold">Expected meat species</span>
                                            <span class="font-bold text-emerald-700">Found</span>
                                            <span class="text-slate-600">Record</span>
                                        </div>
                                        <div class="grid grid-cols-[1fr_0.8fr_0.9fr] px-4 py-4">
                                            <span class="font-bold">Species of concern</span>
                                            <span class="font-bold text-rose-700">ALERT</span>
                                            <span class="text-slate-600">Confirm</span>
                                        </div>
                                        <div class="grid grid-cols-[1fr_0.8fr_0.9fr] px-4 py-4">
                                            <span class="font-bold">Control samples</span>
                                            <span class="font-bold text-amber-700">REVIEW</span>
                                            <span class="text-slate-600">Check batch</span>
                                        </div>
                                    </div>
                                </div>

                                <div class="mt-5 rounded-2xl bg-slate-950 p-4 text-white">
                                    <p class="text-xs font-extrabold uppercase tracking-widest text-brand-200">Report wording</p>
                                    <p class="mt-2 text-sm leading-6 text-slate-300">Shows screening evidence and recommended follow-up. It does not issue religious or legal certification.</p>
                                </div>
                            </div>
                        </div>
                    </div>
                </section>

                <section id="how" class="border-y border-slate-900/10 bg-white">
                    <div class="mx-auto max-w-[1440px] px-4 py-16 sm:px-6 lg:px-8">
                        <div class="grid gap-10 lg:grid-cols-[0.55fr_1fr]">
                            <div>
                                <p class="text-sm font-extrabold uppercase tracking-widest text-brand-700">How it works</p>
                                <h2 class="mt-3 max-w-md text-4xl font-extrabold leading-tight tracking-normal">A simple path from sample to report.</h2>
                            </div>
                            <div class="grid gap-px overflow-hidden rounded-3xl border border-slate-900/10 bg-slate-900/10 md:grid-cols-4">
                                @foreach ([
                                    ['1', 'Prepare the meat sample', 'The lab takes the meat product through its normal sample preparation process.'],
                                    ['2', 'Upload sequencing files', 'Add the files produced from that meat sample.'],
                                    ['3', 'Review result', 'See CLEAR, ALERT, or REVIEW with plain notes.'],
                                    ['4', 'Export report', 'Download the lab report and evidence package.'],
                                ] as [$step, $title, $body])
                                    <div class="bg-[#f7f8f4] p-5">
                                        <p class="font-mono text-sm font-extrabold text-brand-700">{{ $step }}</p>
                                        <h3 class="mt-4 text-lg font-extrabold text-slate-950">{{ $title }}</h3>
                                        <p class="mt-2 text-sm leading-6 text-slate-600">{{ $body }}</p>
                                    </div>
                                @endforeach
                            </div>
                        </div>
                    </div>
                </section>

                <section class="mx-auto max-w-[1440px] px-4 py-16 sm:px-6 lg:px-8">
                    <div class="grid gap-8 lg:grid-cols-[0.8fr_1fr]">
                        <div>
                            <p class="text-sm font-extrabold uppercase tracking-widest text-brand-700">Why labs use it</p>
                            <h2 class="mt-3 text-4xl font-extrabold leading-tight tracking-normal">Built for quick screening before deeper confirmation.</h2>
                        </div>
                        <div class="grid gap-4 md:grid-cols-2">
                            @foreach ([
                                ['Faster triage', 'Know which samples are likely fine and which need attention.'],
                                ['Clear lab language', 'Simple result classes and recommended next steps.'],
                                ['Control checks', 'Shows when a batch may need review because of control signals.'],
                                ['Audit-ready reports', 'Keeps the result, files, settings, and checks together.'],
                            ] as [$title, $body])
                                <article class="border-t border-slate-900/15 pt-4">
                                    <h3 class="font-extrabold text-slate-950">{{ $title }}</h3>
                                    <p class="mt-2 text-sm leading-6 text-slate-600">{{ $body }}</p>
                                </article>
                            @endforeach
                        </div>
                    </div>
                </section>

                <section id="reports" class="bg-slate-950 text-white">
                    <div class="mx-auto grid max-w-[1440px] gap-10 px-4 py-16 sm:px-6 lg:grid-cols-[0.7fr_1fr] lg:px-8">
                        <div>
                            <p class="text-sm font-extrabold uppercase tracking-widest text-brand-200">Reports</p>
                            <h2 class="mt-3 text-4xl font-extrabold leading-tight tracking-normal">Plain enough for review, detailed enough for audit.</h2>
                            <p class="mt-5 text-sm leading-6 text-slate-300">Each report explains what was checked, what was found, whether controls need attention, and whether confirmatory testing is recommended.</p>
                        </div>
                        <div class="grid gap-3 sm:grid-cols-2 lg:grid-cols-4">
                            @foreach (['Sample summary', 'Control notes', 'Species table', 'Follow-up recommendation', 'PDF report', 'JSON result', 'TSV table', 'ZIP package'] as $item)
                                <div class="min-h-24 rounded-2xl border border-white/10 bg-white/[0.06] p-4 text-sm font-bold">{{ $item }}</div>
                            @endforeach
                        </div>
                    </div>
                </section>
            </main>
        </div>
    </body>
</html>
