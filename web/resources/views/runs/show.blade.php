@extends('layouts.app')
@section('title', $run->label)
@section('content')
<div class="flex justify-between items-start mb-6">
    <div>
        <h1 class="text-2xl font-bold text-gray-900">{{ $run->label }}</h1>
        <p class="text-sm text-gray-500 mt-1">Run ID: {{ $run->uuid }}</p>
    </div>
    <div class="flex items-center space-x-2">
        @if($run->status === 'completed')
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium
                @if($run->run_qc_status === 'RUN_PASS') bg-green-100 text-green-800
                @elseif($run->run_qc_status === 'RUN_REVIEW') bg-yellow-100 text-yellow-800
                @else bg-red-100 text-red-800
                @endif">
                {{ $run->run_qc_status ?? 'N/A' }}
            </span>
            <a href="{{ route('reports.run.zip', $run) }}" class="text-sm text-brand-600 hover:text-brand-700 font-medium">Download ZIP</a>
        @elseif($run->status === 'failed')
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-red-100 text-red-800">Failed</span>
        @else
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-blue-100 text-blue-800">
                {{ ucfirst($run->status) }}
            </span>
        @endif
    </div>
</div>

<!-- Run Info -->
<div class="bg-white rounded-lg shadow p-6 mb-6">
    <div class="grid sm:grid-cols-3 gap-4 text-sm">
        <div><span class="text-gray-500">Database:</span> <span class="font-medium">{{ $run->referenceDatabase->name }} v{{ $run->referenceDatabase->version }}</span></div>
        <div><span class="text-gray-500">Calibration:</span> <span class="font-medium">{{ $run->calibrationProfile?->name ?? 'None' }}</span></div>
        <div><span class="text-gray-500">Created:</span> <span class="font-medium">{{ $run->created_at->format('Y-m-d H:i') }}</span></div>
        <div><span class="text-gray-500">Markers:</span> <span class="font-medium">{{ implode(', ', $run->marker_panel ?? []) }}</span></div>
        <div><span class="text-gray-500">Total Reads:</span> <span class="font-medium">{{ number_format($run->total_reads ?? 0) }}</span></div>
        <div><span class="text-gray-500">Classified:</span> <span class="font-medium">{{ isset($run->classified_read_pct) ? round($run->classified_read_pct, 1).'%' : 'N/A' }}</span></div>
        @if($run->computation_time_seconds)
            <div><span class="text-gray-500">Analysis Time:</span> <span class="font-medium">{{ round($run->computation_time_seconds, 1) }}s</span></div>
        @endif
        @if($run->error_message)
            <div class="sm:col-span-3"><span class="text-gray-500">Error:</span> <span class="text-red-600">{{ $run->error_message }}</span></div>
        @endif
    </div>
</div>

<!-- QC Summary -->
@if($run->run_qc_summary)
<div class="bg-white rounded-lg shadow p-6 mb-6">
    <h2 class="text-lg font-semibold text-gray-800 mb-4">Run QC</h2>
    <div class="grid sm:grid-cols-3 gap-4 text-sm">
        <div><span class="text-gray-500">Status:</span> <span class="font-medium">{{ $run->run_qc_summary['status'] ?? 'N/A' }}</span></div>
        <div><span class="text-gray-500">Index Bleed:</span> <span class="font-medium">{{ isset($run->run_qc_summary['index_bleed_estimate']) ? round($run->run_qc_summary['index_bleed_estimate'] * 100, 2).'%' : 'N/A' }}</span></div>
    </div>
    @if(!empty($run->run_qc_summary['warnings']))
        <div class="mt-3 space-y-1">
            @foreach($run->run_qc_summary['warnings'] as $warning)
                <div class="text-sm text-yellow-700 bg-yellow-50 border border-yellow-200 rounded px-3 py-1.5">{{ $warning }}</div>
            @endforeach
        </div>
    @endif
</div>
@endif

<!-- Samples -->
<h2 class="text-lg font-semibold text-gray-800 mb-3">Samples ({{ $run->samples->count() }})</h2>
<div class="bg-white rounded-lg shadow overflow-hidden">
    <table class="min-w-full divide-y divide-gray-200 text-sm">
        <thead class="bg-gray-50">
            <tr>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Sample</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Role</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Result</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Reads</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Actions</th>
            </tr>
        </thead>
        <tbody class="divide-y divide-gray-100">
            @foreach($run->samples as $sample)
                <tr class="hover:bg-gray-50">
                    <td class="px-4 py-3 font-medium text-gray-900">{{ $sample->label ?: $sample->sample_id }}</td>
                    <td class="px-4 py-3">
                        <span class="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium
                            @if($sample->role === 'sample') bg-blue-100 text-blue-800
                            @elseif($sample->role === 'positive_control') bg-green-100 text-green-800
                            @elseif($sample->role === 'negative_control') bg-gray-100 text-gray-800
                            @else bg-purple-100 text-purple-800
                            @endif">
                            {{ str_replace('_', ' ', $sample->role) }}
                        </span>
                    </td>
                    <td class="px-4 py-3">
                        @if($sample->screening_result === 'CLEAR')
                            <span class="text-green-700 font-medium">CLEAR</span>
                        @elseif($sample->screening_result === 'ALERT')
                            <span class="text-red-700 font-medium">ALERT</span>
                        @elseif($sample->screening_result === 'REVIEW')
                            <span class="text-yellow-700 font-medium">REVIEW</span>
                        @elseif($sample->screening_result === 'CONTROL_CONTAMINATION')
                            <span class="text-orange-700 font-medium">CONTROL CONTAMINATION</span>
                        @else
                            <span class="text-gray-400">{{ $sample->screening_result ?? 'Pending' }}</span>
                        @endif
                    </td>
                    <td class="px-4 py-3 text-gray-500">{{ number_format($sample->total_reads ?? 0) }}</td>
                    <td class="px-4 py-3">
                        <a href="{{ route('samples.show', $sample) }}" class="text-brand-600 hover:text-brand-700 text-xs font-medium">View</a>
                    </td>
                </tr>
            @endforeach
        </tbody>
    </table>
</div>
@endsection
