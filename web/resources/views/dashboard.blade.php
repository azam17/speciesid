@extends('layouts.app')

@section('title', 'Dashboard | SpeciesID')

@section('content')
<div class="mx-auto max-w-[1480px] px-4 py-6 sm:px-6 lg:px-8">
    <div class="grid gap-5 lg:grid-cols-[260px_minmax(0,1fr)]">
        <aside class="hidden border-r border-slate-900/10 pr-5 lg:block">
            <p class="text-xs font-bold uppercase tracking-widest text-brand-700">Console</p>
            <h1 class="mt-3 text-3xl font-extrabold leading-tight tracking-normal text-slate-950">SpeciesID Review Bench</h1>
            <p class="mt-4 text-sm leading-6 text-slate-600">Operational view for run QC, sample review queues, active reference assets, and audit exports.</p>

            <div class="mt-8 space-y-4 border-y border-slate-900/10 py-5 text-sm">
                <div>
                    <p class="text-slate-500">Active database</p>
                    <p class="mt-1 font-extrabold text-slate-950">{{ $stats['active_database'] }}</p>
                </div>
                <div>
                    <p class="text-slate-500">Calibration profile</p>
                    <p class="mt-1 font-extrabold text-slate-950">{{ $stats['active_calibration'] }}</p>
                </div>
            </div>

            <div class="mt-5 grid gap-2">
                <a href="{{ route('upload.create') }}" class="rounded-full bg-slate-950 px-4 py-3 text-center text-sm font-bold text-white hover:bg-brand-800">New analysis run</a>
                <a href="{{ route('runs.index') }}" class="rounded-full border border-slate-900/10 bg-white px-4 py-3 text-center text-sm font-bold text-slate-900 shadow-sm hover:border-brand-400">Open run ledger</a>
            </div>
        </aside>

        <main class="min-w-0">
            <div class="mb-5 flex flex-col gap-4 border-b border-slate-900/10 pb-5 lg:hidden">
                <div>
                    <p class="text-xs font-bold uppercase tracking-widest text-brand-700">Console</p>
                    <h1 class="mt-2 text-3xl font-extrabold tracking-normal text-slate-950">SpeciesID Review Bench</h1>
                </div>
                <div class="flex flex-col gap-2 sm:flex-row">
                    <a href="{{ route('upload.create') }}" class="rounded-full bg-slate-950 px-4 py-3 text-center text-sm font-bold text-white hover:bg-brand-800">New analysis run</a>
                    <a href="{{ route('runs.index') }}" class="rounded-full border border-slate-900/10 bg-white px-4 py-3 text-center text-sm font-bold text-slate-900 shadow-sm">Open run ledger</a>
                </div>
            </div>

            <section class="grid gap-px overflow-hidden rounded-[24px] border border-slate-900/10 bg-slate-900/10 sm:grid-cols-2 xl:grid-cols-4">
                @foreach ([
                    ['label' => 'Total runs', 'value' => $stats['total_runs'], 'tone' => 'text-slate-950', 'note' => 'All submitted batches'],
                    ['label' => 'Runs this week', 'value' => $stats['runs_this_week'], 'tone' => 'text-brand-700', 'note' => 'Recent throughput'],
                    ['label' => 'Needs review', 'value' => $stats['pending_review'], 'tone' => 'text-amber-700', 'note' => 'Attention queue'],
                    ['label' => 'Failed controls', 'value' => $stats['failed_controls'], 'tone' => 'text-rose-700', 'note' => 'Control exceptions'],
                ] as $metric)
                    <div class="bg-white p-5">
                        <p class="text-xs font-bold uppercase tracking-widest text-slate-500">{{ $metric['label'] }}</p>
                        <p class="mt-3 text-4xl font-extrabold {{ $metric['tone'] }}">{{ $metric['value'] }}</p>
                        <p class="mt-2 text-sm text-slate-500">{{ $metric['note'] }}</p>
                    </div>
                @endforeach
            </section>

            <div class="mt-5 grid gap-5 xl:grid-cols-[minmax(0,1fr)_380px]">
                <section class="overflow-hidden rounded-[24px] border border-slate-900/10 bg-white shadow-sm">
                    <div class="flex flex-col gap-3 border-b border-slate-200 px-5 py-4 sm:flex-row sm:items-center sm:justify-between">
                        <div>
                            <p class="text-xs font-bold uppercase tracking-widest text-brand-700">Sample runs</p>
                            <h2 class="mt-1 text-xl font-extrabold text-slate-950">Recent checks</h2>
                        </div>
                        <a href="{{ route('runs.index') }}" class="text-sm font-bold text-brand-700 hover:text-brand-800">View all</a>
                    </div>
                    <div class="overflow-x-auto">
                        <table class="min-w-full divide-y divide-slate-200 text-sm">
                            <thead class="bg-slate-50 text-left text-xs font-bold uppercase tracking-widest text-slate-500">
                                <tr>
                                    <th class="px-5 py-3">Run</th>
                                    <th class="px-5 py-3">Analysis</th>
                                    <th class="px-5 py-3">QC</th>
                                    <th class="px-5 py-3 text-right">Created</th>
                                </tr>
                            </thead>
                            <tbody class="divide-y divide-slate-100">
                                @forelse($recentRuns as $run)
                                    @php
                                        $qcClass = match ($run->run_qc_status) {
                                            'RUN_PASS' => 'bg-emerald-50 text-emerald-700 ring-emerald-200',
                                            'RUN_FAIL' => 'bg-rose-50 text-rose-700 ring-rose-200',
                                            'RUN_REVIEW' => 'bg-amber-50 text-amber-800 ring-amber-200',
                                            default => 'bg-slate-100 text-slate-700 ring-slate-200',
                                        };
                                    @endphp
                                    <tr class="hover:bg-slate-50">
                                        <td class="px-5 py-4">
                                            <a class="font-extrabold text-slate-950 hover:text-brand-700" href="{{ route('runs.show', $run) }}">{{ $run->label ?: $run->uuid }}</a>
                                            <p class="mt-1 max-w-sm truncate font-mono text-xs text-slate-500">{{ $run->uuid }}</p>
                                        </td>
                                        <td class="px-5 py-4 font-semibold text-slate-600">{{ $run->status }}</td>
                                        <td class="px-5 py-4">
                                            <span class="inline-flex rounded-full px-2.5 py-1 text-xs font-extrabold ring-1 {{ $qcClass }}">{{ $run->run_qc_status ?? 'NOT_REPORTED' }}</span>
                                        </td>
                                        <td class="px-5 py-4 text-right text-slate-500">{{ $run->created_at?->format('Y-m-d H:i') }}</td>
                                    </tr>
                                @empty
                                    <tr>
                                        <td colspan="4" class="px-5 py-12 text-center">
                                            <p class="text-lg font-extrabold text-slate-950">No analysis runs yet.</p>
                                            <p class="mt-2 text-sm text-slate-500">Upload sequencing files prepared from the meat sample to create the first check.</p>
                                        </td>
                                    </tr>
                                @endforelse
                            </tbody>
                        </table>
                    </div>
                </section>

                <aside class="space-y-5">
                    <section class="rounded-[24px] bg-slate-950 p-5 text-white shadow-sm">
                        <p class="text-xs font-bold uppercase tracking-widest text-brand-200">Asset readiness</p>
                        <div class="mt-5 divide-y divide-white/10 border-y border-white/10 text-sm">
                            <div class="py-4">
                                <p class="text-slate-400">Reference database</p>
                                <p class="mt-1 font-extrabold">{{ $stats['active_database'] }}</p>
                            </div>
                            <div class="py-4">
                                <p class="text-slate-400">Calibration profile</p>
                                <p class="mt-1 font-extrabold">{{ $stats['active_calibration'] }}</p>
                            </div>
                        </div>
                        <div class="mt-5 grid grid-cols-2 gap-2">
                            <a class="rounded-full bg-white/10 px-3 py-2 text-center text-sm font-bold hover:bg-white/15" href="{{ route('databases.index') }}">Databases</a>
                            <a class="rounded-full bg-white/10 px-3 py-2 text-center text-sm font-bold hover:bg-white/15" href="{{ route('calibrations.index') }}">Calibrations</a>
                        </div>
                    </section>

                    <section class="rounded-[24px] border border-slate-900/10 bg-white p-5 shadow-sm">
                        <p class="text-xs font-bold uppercase tracking-widest text-brand-700">Run intake rules</p>
                        <div class="mt-4 divide-y divide-slate-200 text-sm">
                            @foreach ([
                                ['At least one sample role', 'Required', 'text-emerald-700 bg-emerald-50'],
                                ['Controls for adjustment', 'Review if absent', 'text-amber-800 bg-amber-50'],
                                ['Quantitative wording', 'Screening estimate', 'text-slate-700 bg-slate-100'],
                            ] as [$label, $status, $class])
                                <div class="flex items-center justify-between gap-3 py-3">
                                    <span class="font-semibold text-slate-700">{{ $label }}</span>
                                    <span class="rounded-full px-2.5 py-1 text-xs font-extrabold {{ $class }}">{{ $status }}</span>
                                </div>
                            @endforeach
                        </div>
                    </section>
                </aside>
            </div>

            <section class="mt-5 overflow-hidden rounded-[24px] border border-slate-900/10 bg-white shadow-sm">
                <div class="border-b border-slate-200 px-5 py-4">
                    <p class="text-xs font-bold uppercase tracking-widest text-brand-700">Review queue</p>
                    <h2 class="mt-1 text-xl font-extrabold text-slate-950">Samples needing review</h2>
                    <p class="mt-1 text-sm text-slate-500">Near-threshold, ambiguous, or control-related screening results.</p>
                </div>
                <div class="overflow-x-auto">
                    <table class="min-w-full divide-y divide-slate-200 text-sm">
                        <thead class="bg-slate-50 text-left text-xs font-bold uppercase tracking-widest text-slate-500">
                            <tr>
                                <th class="px-5 py-3">Sample</th>
                                <th class="px-5 py-3">Screening result</th>
                                <th class="px-5 py-3">Run</th>
                            </tr>
                        </thead>
                        <tbody class="divide-y divide-slate-100">
                            @forelse($pendingReview as $sample)
                                <tr class="hover:bg-slate-50">
                                    <td class="px-5 py-4">
                                        <a class="font-extrabold text-slate-950 hover:text-brand-700" href="{{ route('samples.show', $sample) }}">{{ $sample->label ?: $sample->sample_id }}</a>
                                    </td>
                                    <td class="px-5 py-4 font-semibold text-slate-600">{{ $sample->screening_result }}</td>
                                    <td class="px-5 py-4 text-slate-500">{{ $sample->run->label ?: $sample->run->uuid }}</td>
                                </tr>
                            @empty
                                <tr>
                                    <td colspan="3" class="px-5 py-12 text-center">
                                        <p class="text-lg font-extrabold text-slate-950">No samples currently require review.</p>
                                        <p class="mt-2 text-sm text-slate-500">New ALERT and REVIEW classes will appear here after analysis.</p>
                                    </td>
                                </tr>
                            @endforelse
                        </tbody>
                    </table>
                </div>
            </section>
        </main>
    </div>
</div>
@endsection
