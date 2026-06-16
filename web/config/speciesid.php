<?php

return [
    /*
    |--------------------------------------------------------------------------
    | SpeciesID Engine Configuration
    |--------------------------------------------------------------------------
    */
    'engine' => [
        'binary_path' => env('SPECIESID_BINARY_PATH', base_path('../speciesid')),
        'schema_path' => env('SPECIESID_SCHEMA_PATH', base_path('../schema')),
        'timeout_seconds' => env('SPECIESID_TIMEOUT', 3600),
    ],

    /*
    |--------------------------------------------------------------------------
    | Storage Disks
    |--------------------------------------------------------------------------
    */
    'storage' => [
        'fastq_disk' => env('SPECIESID_FASTQ_DISK', 'fastq'),
        'index_disk' => env('SPECIESID_INDEX_DISK', 'indexes'),
        'calibration_disk' => env('SPECIESID_CALIBRATION_DISK', 'calibrations'),
    ],

    /*
    |--------------------------------------------------------------------------
    | Default Analysis Parameters
    |--------------------------------------------------------------------------
    */
    'defaults' => [
        'detection_threshold' => 0.001,
        'prune_threshold' => 0.0,
        'n_bootstrap' => 0,
        'read_mode' => 'auto',
        'control_adjustment' => true,
        'out_of_panel_check' => true,
        'resolvability_check' => true,
    ],

    /*
    |--------------------------------------------------------------------------
    | Report Configuration
    |--------------------------------------------------------------------------
    */
    'reports' => [
        'pdf_watermark' => env('SPECIESID_PDF_WATERMARK', 'SpeciesID Screening Report — Not a Certificate'),
        'footer_text' => 'This report provides molecular screening evidence. It does not constitute legal or religious certification. Confirmatory testing by accredited methods is recommended for species-of-concern detections near configured evidence thresholds.',
    ],

    /*
    |--------------------------------------------------------------------------
    | Terminology Mapping
    |--------------------------------------------------------------------------
    | Maps internal codes to user-facing display labels.
    */
    'terminology' => [
        'screening_results' => [
            'CLEAR' => 'CLEAR',
            'ALERT' => 'ALERT',
            'REVIEW' => 'REVIEW',
            'RUN_FAIL' => 'RUN_FAIL',
            'CONTROL_CONTAMINATION' => 'CONTROL_CONTAMINATION',
        ],
        'confidence_classes' => [
            'HIGH' => 'High Confidence',
            'MEDIUM' => 'Medium Confidence',
            'LOW' => 'Low Confidence',
            'TRACE' => 'Trace Level',
            'CONTROL_CONSISTENT' => 'Consistent with Controls',
            'AMBIGUOUS_SPECIES' => 'Ambiguous Species',
            'OUT_OF_PANEL_RISK' => 'Out-of-Panel Risk',
        ],
        'run_qc_statuses' => [
            'RUN_PASS' => 'Pass',
            'RUN_REVIEW' => 'Review',
            'RUN_FAIL' => 'Fail',
        ],
    ],
];
