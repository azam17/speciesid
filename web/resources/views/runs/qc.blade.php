@extends('layouts.app')
@section('title', 'Run QC')
@section('content')
<div class="max-w-5xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900 mb-2">Run QC</h1>
    <p class="text-sm text-gray-500 mb-6">{{ $run->label }} · {{ $run->uuid }}</p>
    <div class="bg-white rounded-lg shadow p-6 space-y-4">
        <div class="grid sm:grid-cols-3 gap-4 text-sm">
            <div><span class="text-gray-500">Status</span><br><span class="font-medium">{{ $run->run_qc_status ?? 'Pending' }}</span></div>
            <div><span class="text-gray-500">Total reads</span><br><span class="font-medium">{{ number_format($run->total_reads ?? 0) }}</span></div>
            <div><span class="text-gray-500">Classified</span><br><span class="font-medium">{{ isset($run->classified_read_pct) ? round($run->classified_read_pct, 2).'%' : 'N/A' }}</span></div>
        </div>
        @foreach(($run->run_qc_summary['warnings'] ?? []) as $warning)
            <div class="bg-yellow-50 border border-yellow-200 text-yellow-800 rounded px-3 py-2 text-sm">{{ $warning }}</div>
        @endforeach
        <h2 class="text-lg font-semibold">Sample Depth</h2>
        <table class="min-w-full divide-y divide-gray-200 text-sm">
            <thead><tr><th class="py-2 text-left">Sample</th><th class="py-2 text-right">Reads</th><th class="py-2 text-right">Classified</th><th class="py-2 text-right">Unclassified</th></tr></thead>
            <tbody>
                @foreach(($run->run_qc_summary['per_sample_depth'] ?? []) as $row)
                    <tr class="border-t"><td class="py-2">{{ $row['sample_id'] ?? 'N/A' }}</td><td class="py-2 text-right">{{ number_format($row['n_reads'] ?? 0) }}</td><td class="py-2 text-right">{{ number_format($row['classified'] ?? 0) }}</td><td class="py-2 text-right">{{ number_format($row['unclassified'] ?? 0) }}</td></tr>
                @endforeach
            </tbody>
        </table>
    </div>
</div>
@endsection
