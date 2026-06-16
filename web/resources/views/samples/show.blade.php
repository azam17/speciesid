@extends('layouts.app')
@section('title', $sample->label ?: $sample->sample_id)
@section('content')
<div class="flex justify-between items-start mb-6">
    <div>
        <div class="flex items-center space-x-2">
            <h1 class="text-2xl font-bold text-gray-900">{{ $sample->label ?: $sample->sample_id }}</h1>
            <span class="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium
                @if($sample->role === 'sample') bg-blue-100 text-blue-800
                @elseif($sample->role === 'positive_control') bg-green-100 text-green-800
                @else bg-gray-100 text-gray-800
                @endif">
                {{ str_replace('_', ' ', $sample->role) }}
            </span>
        </div>
        <p class="text-sm text-gray-500 mt-1">Run: <a href="{{ route('runs.show', $sample->run) }}" class="text-brand-600 hover:underline">{{ $sample->run->label }}</a></p>
    </div>
    <div class="flex items-center space-x-2">
        @if($sample->screening_result === 'CLEAR')
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-green-100 text-green-800">CLEAR</span>
        @elseif($sample->screening_result === 'ALERT')
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-red-100 text-red-800">ALERT</span>
        @elseif($sample->screening_result === 'REVIEW')
            <span class="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-yellow-100 text-yellow-800">REVIEW</span>
        @endif

        <a href="{{ route('reports.sample.json', $sample) }}" class="text-xs bg-gray-100 hover:bg-gray-200 px-2 py-1 rounded">JSON</a>
        <a href="{{ route('reports.sample.tsv', $sample) }}" class="text-xs bg-gray-100 hover:bg-gray-200 px-2 py-1 rounded">TSV</a>
        <a href="{{ route('reports.sample.pdf', $sample) }}" class="text-xs bg-gray-100 hover:bg-gray-200 px-2 py-1 rounded">PDF</a>
    </div>
</div>

<!-- Evidence Summary -->
@if($sample->evidence_summary)
<div class="bg-white rounded-lg shadow p-6 mb-6">
    <h2 class="text-lg font-semibold text-gray-800 mb-3">Evidence Summary</h2>
    <div class="grid sm:grid-cols-4 gap-4 text-sm">
        <div><span class="text-gray-500">Total Reads:</span> <span class="font-medium">{{ number_format($sample->evidence_summary['total_reads'] ?? 0) }}</span></div>
        <div><span class="text-gray-500">Classified:</span> <span class="font-medium">{{ number_format($sample->evidence_summary['classified_reads'] ?? 0) }}</span></div>
        <div><span class="text-gray-500">Species Detected:</span> <span class="font-medium">{{ $sample->evidence_summary['n_species_detected'] ?? 'N/A' }}</span></div>
        <div><span class="text-gray-500">Evidence Threshold Estimate:</span> <span class="font-medium">{{ isset($sample->evidence_summary['evidence_threshold_estimate_pct']) ? round($sample->evidence_summary['evidence_threshold_estimate_pct'], 2).'%' : 'N/A' }}</span></div>
    </div>

    @if($sample->out_of_panel_flag)
        <div class="mt-3 p-3 bg-yellow-50 border border-yellow-200 rounded text-sm text-yellow-800">
            Out-of-panel risk detected. {{ $sample->out_of_panel_details['recommendation'] ?? 'CONFIRMATORY_TEST_RECOMMENDED' }}
        </div>
    @endif
</div>
@endif

<!-- Species Table -->
@if($sample->species_results)
<div class="bg-white rounded-lg shadow overflow-hidden">
    <div class="px-4 py-3 border-b border-gray-200">
        <h2 class="text-lg font-semibold text-gray-800">Species Results</h2>
    </div>
    <table class="min-w-full divide-y divide-gray-200 text-sm">
        <thead class="bg-gray-50">
            <tr>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Species</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Category</th>
                <th class="px-4 py-3 text-right text-xs font-medium text-gray-500 uppercase">Estimate (w/w%)</th>
                <th class="px-4 py-3 text-right text-xs font-medium text-gray-500 uppercase">95% CI</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Confidence</th>
                <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">p-value</th>
            </tr>
        </thead>
        <tbody class="divide-y divide-gray-100">
            @foreach($sample->species_results as $sp)
                @php
                    $confidenceColors = [
                        'HIGH' => 'bg-green-100 text-green-800',
                        'MEDIUM' => 'bg-blue-100 text-blue-800',
                        'LOW' => 'bg-yellow-100 text-yellow-800',
                        'TRACE' => 'bg-gray-100 text-gray-600',
                        'CONTROL_CONSISTENT' => 'bg-orange-100 text-orange-800',
                        'AMBIGUOUS_SPECIES' => 'bg-purple-100 text-purple-800',
                        'OUT_OF_PANEL_RISK' => 'bg-red-100 text-red-800',
                    ];
                    $cc = $confidenceColors[$sp['confidence_class'] ?? 'TRACE'] ?? 'bg-gray-100 text-gray-600';
                @endphp
                <tr class="hover:bg-gray-50">
                    <td class="px-4 py-3">
                        <div class="font-medium text-gray-900">{{ $sp['species_id'] ?? 'Unknown' }}</div>
                        @if($sp['common_name'] ?? false)
                            <div class="text-xs text-gray-500">{{ $sp['common_name'] }}</div>
                        @endif
                    </td>
                    <td class="px-4 py-3">
                        <span class="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium
                            @if(($sp['species_category'] ?? '') === 'EXCLUSION') bg-red-100 text-red-800
                            @elseif(($sp['species_category'] ?? '') === 'REVIEW') bg-yellow-100 text-yellow-800
                            @else bg-green-100 text-green-800
                            @endif">
                            {{ $sp['species_category'] ?? 'REFERENCE' }}
                        </span>
                    </td>
                    <td class="px-4 py-3 text-right font-mono text-sm">
                        {{ isset($sp['screening_estimate_pct']) ? number_format($sp['screening_estimate_pct'], 2) : '—' }}%
                    </td>
                    <td class="px-4 py-3 text-right font-mono text-xs text-gray-500">
                        {{ isset($sp['ci_lo_pct']) ? number_format($sp['ci_lo_pct'], 2) : '?' }}% – {{ isset($sp['ci_hi_pct']) ? number_format($sp['ci_hi_pct'], 2) : '?' }}%
                    </td>
                    <td class="px-4 py-3">
                        <span class="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium {{ $cc }}">
                            {{ $sp['confidence_class'] ?? 'N/A' }}
                        </span>
                    </td>
                    <td class="px-4 py-3 font-mono text-xs text-gray-500">
                        {{ isset($sp['p_value']) ? sprintf('%.2e', $sp['p_value']) : '—' }}
                    </td>
                </tr>
                @if(!empty($sp['ambiguity_group']))
                    <tr class="bg-purple-50">
                        <td colspan="6" class="px-4 py-2 text-xs text-purple-700">
                            Ambiguous with: {{ implode(', ', $sp['ambiguity_group']) }} — {{ $sp['ambiguity_note'] ?? 'Marker cannot resolve these species' }}
                        </td>
                    </tr>
                @endif
                @if($sp['confirmatory_recommended'] ?? false)
                    <tr class="bg-yellow-50">
                        <td colspan="6" class="px-4 py-2 text-xs text-yellow-700">
                            Confirmatory test recommended. {{ $sp['confirmatory_reason'] ?? '' }}
                        </td>
                    </tr>
                @endif
            @endforeach
        </tbody>
    </table>
</div>

<!-- Marker Evidence -->
<div class="bg-white rounded-lg shadow overflow-hidden mt-6">
    <div class="px-4 py-3 border-b border-gray-200">
        <h2 class="text-lg font-semibold text-gray-800">Marker-Level Evidence</h2>
    </div>
    <div class="overflow-x-auto">
        <table class="min-w-full divide-y divide-gray-200 text-sm">
            <thead class="bg-gray-50">
                <tr>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Species</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Marker</th>
                    <th class="px-4 py-3 text-right text-xs font-medium text-gray-500 uppercase">Reads</th>
                    <th class="px-4 py-3 text-right text-xs font-medium text-gray-500 uppercase">Containment</th>
                    <th class="px-4 py-3 text-right text-xs font-medium text-gray-500 uppercase">Uniqueness</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Status</th>
                </tr>
            </thead>
            <tbody class="divide-y divide-gray-100">
                @foreach($sample->species_results as $sp)
                    @foreach($sp['marker_evidence'] ?? [] as $me)
                        <tr class="hover:bg-gray-50">
                            <td class="px-4 py-2 text-gray-900">{{ $sp['species_id'] ?? '?' }}</td>
                            <td class="px-4 py-2 font-mono text-xs">{{ $me['marker_id'] ?? '?' }}</td>
                            <td class="px-4 py-2 text-right font-mono">{{ $me['read_count'] ?? 0 }}</td>
                            <td class="px-4 py-2 text-right font-mono text-xs">{{ isset($me['containment_mean']) ? number_format($me['containment_mean'], 3) : '—' }}</td>
                            <td class="px-4 py-2 text-right font-mono text-xs">{{ isset($me['kmer_uniqueness_score']) ? number_format($me['kmer_uniqueness_score'], 3) : '—' }}</td>
                            <td class="px-4 py-2">
                                <span class="inline-flex items-center px-2 py-0.5 rounded text-xs font-medium
                                    @if(($me['detection_status'] ?? '') === 'DETECTED') bg-green-100 text-green-800
                                    @elseif(($me['detection_status'] ?? '') === 'AMBIGUOUS') bg-purple-100 text-purple-800
                                    @else bg-gray-100 text-gray-600
                                    @endif">
                                    {{ $me['detection_status'] ?? 'N/A' }}
                                </span>
                            </td>
                        </tr>
                    @endforeach
                @endforeach
            </tbody>
        </table>
    </div>
</div>
@endif

<div class="mt-8 p-4 bg-gray-50 border border-gray-200 rounded-lg text-xs text-gray-500">
    <strong>Disclaimer:</strong> This report provides molecular screening evidence from amplicon sequencing. Weight-fraction estimates are {{ $sample->run->calibrationProfile ? 'calibrated' : 'uncalibrated screening estimates' }} and should not be interpreted as certified quantitative results without independent validation. Confirmatory testing by accredited methods (species-specific PCR, ddPCR, or Sanger sequencing) is recommended for EXCLUSION-category detections near configured evidence thresholds.
</div>
@endsection
