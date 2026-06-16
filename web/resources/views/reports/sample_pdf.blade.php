<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>SpeciesID Report — {{ $sample->sample_id }}</title>
    <style>
        body { font-family: Helvetica, Arial, sans-serif; font-size: 11pt; color: #1a1a1a; line-height: 1.5; margin: 40px; }
        h1 { font-size: 18pt; margin-bottom: 4pt; }
        .subtitle { color: #666; font-size: 9pt; margin-bottom: 20pt; }
        .qc-badge { display: inline-block; padding: 3px 10px; border-radius: 12px; font-size: 10pt; font-weight: bold; }
        .qc-pass { background: #dcfce7; color: #166534; }
        .qc-review { background: #fef3c7; color: #92400e; }
        .qc-fail { background: #fee2e2; color: #991b1b; }
        .section { margin-bottom: 20pt; }
        .section h2 { font-size: 13pt; border-bottom: 2px solid #16a34a; padding-bottom: 4pt; margin-bottom: 10pt; }
        table { width: 100%; border-collapse: collapse; font-size: 9pt; }
        th { background: #f3f4f6; text-align: left; padding: 6px 8px; font-weight: 600; text-transform: uppercase; font-size: 8pt; color: #666; }
        td { padding: 6px 8px; border-bottom: 1px solid #e5e7eb; }
        .species-alert { background: #fef2f2; }
        .info-grid { display: flex; flex-wrap: wrap; gap: 12px 30px; font-size: 9pt; }
        .info-item { min-width: 160px; }
        .info-label { color: #666; font-size: 8pt; text-transform: uppercase; }
        .footer { margin-top: 30pt; padding-top: 10pt; border-top: 1px solid #ddd; font-size: 7.5pt; color: #999; }
        .disclaimer { background: #f9fafb; border: 1px solid #e5e7eb; padding: 10pt; font-size: 8pt; color: #666; margin-top: 20pt; }
        .page-break { page-break-after: always; }
    </style>
</head>
<body>
    <h1>SpeciesID Screening Report</h1>
    <p class="subtitle">
        Sample: {{ $sample->label ?: $sample->sample_id }} &nbsp;|&nbsp;
        Role: {{ str_replace('_', ' ', $sample->role) }} &nbsp;|&nbsp;
        Generated: {{ $generated_at }} &nbsp;|&nbsp;
        Engine: v0.2.0
    </p>

    <div class="qc-badge qc-{{ strtolower(str_replace('_', '-', $run_qc ?? 'review')) }}">
        Run QC: {{ $run_qc ?? 'N/A' }}
    </div>

    @if($sample->screening_result === 'ALERT')
        <div style="background: #fef2f2; border: 2px solid #ef4444; padding: 12pt; margin-top: 12pt; border-radius: 6pt;">
            <strong style="color: #991b1b;">ALERT</strong> —
            Species-of-concern detected at screening level. Confirmatory testing recommended.
        </div>
    @elseif($sample->screening_result === 'REVIEW')
        <div style="background: #fffbeb; border: 2px solid #f59e0b; padding: 12pt; margin-top: 12pt; border-radius: 6pt;">
            <strong style="color: #92400e;">REVIEW</strong> —
            Results require manual review. See ambiguity notes and low-confidence detections.
        </div>
    @endif

    <!-- Evidence Summary -->
    <div class="section">
        <h2>Evidence Summary</h2>
        <div class="info-grid">
            <div class="info-item"><span class="info-label">Total reads</span><br>{{ number_format($summary['total_reads'] ?? 0) }}</div>
            <div class="info-item"><span class="info-label">Classified reads</span><br>{{ number_format($summary['classified_reads'] ?? 0) }}</div>
            <div class="info-item"><span class="info-label">Species detected</span><br>{{ $summary['n_species_detected'] ?? 'N/A' }}</div>
            <div class="info-item"><span class="info-label">Active markers</span><br>{{ $summary['n_markers_active'] ?? 'N/A' }}</div>
            <div class="info-item"><span class="info-label">Evidence threshold estimate</span><br>{{ isset($summary['evidence_threshold_estimate_pct']) ? round($summary['evidence_threshold_estimate_pct'], 2).'%' : 'N/A' }}</div>
            <div class="info-item"><span class="info-label">Database version</span><br>{{ $database->version ?? 'N/A' }}</div>
            <div class="info-item"><span class="info-label">Calibration</span><br>{{ $calibration->version ?? 'None' }}</div>
        </div>
    </div>

    <!-- Species Results -->
    <div class="section">
        <h2>Species Results</h2>
        <table>
            <thead>
                <tr>
                    <th>Species</th>
                    <th>Category</th>
                    <th>Estimate (w/w%)</th>
                    <th>95% CI</th>
                    <th>Confidence</th>
                    <th>p-value</th>
                    <th>Ambiguity</th>
                </tr>
            </thead>
            <tbody>
                @foreach($species as $sp)
                <tr class="{{ ($sp['species_category'] ?? '') === 'EXCLUSION' ? 'species-alert' : '' }}">
                    <td><strong>{{ $sp['species_id'] ?? '?' }}</strong>{{ isset($sp['common_name']) ? ' ('.$sp['common_name'].')' : '' }}</td>
                    <td>{{ $sp['species_category'] ?? '—' }}</td>
                    <td>{{ isset($sp['screening_estimate_pct']) ? number_format($sp['screening_estimate_pct'], 3) : '—' }}%</td>
                    <td>{{ isset($sp['ci_lo_pct']) ? number_format($sp['ci_lo_pct'], 3) : '?' }}% – {{ isset($sp['ci_hi_pct']) ? number_format($sp['ci_hi_pct'], 3) : '?' }}%</td>
                    <td>{{ $sp['confidence_class'] ?? '—' }}</td>
                    <td>{{ isset($sp['p_value']) ? sprintf('%.2e', $sp['p_value']) : '—' }}</td>
                    <td>{{ implode(', ', $sp['ambiguity_group'] ?? []) ?: '—' }}</td>
                </tr>
                @endforeach
            </tbody>
        </table>
    </div>

    <div class="disclaimer">
        <strong>Important:</strong> This report provides molecular screening evidence from amplicon sequencing. Weight-fraction estimates are {{ $calibration ? 'calibrated' : 'uncalibrated screening estimates' }} and should not be interpreted as certified quantitative results. SpeciesID does not issue religious or legal certification. Confirmatory testing by accredited methods is recommended for species-of-concern detections near configured evidence thresholds or with confidence class LOW/TRACE/AMBIGUOUS_SPECIES.
    </div>

    <div class="footer">
        SpeciesID v0.2.0 &nbsp;|&nbsp; Database: {{ $database->sha256_hash ?? 'N/A' }} &nbsp;|&nbsp; This is a screening report, not a certificate.
    </div>
</body>
</html>
