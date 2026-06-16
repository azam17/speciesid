@extends('layouts.app')
@section('title', 'New Analysis Run')
@section('content')
<h1 class="text-2xl font-bold text-gray-900 mb-6">New Analysis Run</h1>

<form action="{{ route('upload.store') }}" method="POST" enctype="multipart/form-data" class="space-y-6 max-w-3xl">
    @csrf

    <!-- Run Info -->
    <div class="bg-white rounded-lg shadow p-6 space-y-4">
        <h2 class="text-lg font-semibold text-gray-800">Run Configuration</h2>

        <div>
            <label class="block text-sm font-medium text-gray-700">Run Label</label>
            <input type="text" name="label" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm" placeholder="e.g. Batch 2026-06-16">
        </div>

        <div class="grid sm:grid-cols-2 gap-4">
            <div>
                <label class="block text-sm font-medium text-gray-700">Reference Database</label>
                <select name="reference_database_id" required class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                    <option value="">Select database...</option>
                    @foreach ($databases as $db)
                        <option value="{{ $db->id }}" {{ $db->is_active ? 'selected' : '' }}>
                            {{ $db->name }} v{{ $db->version }}
                        </option>
                    @endforeach
                </select>
            </div>
            <div>
                <label class="block text-sm font-medium text-gray-700">Calibration Profile (optional)</label>
                <select name="calibration_profile_id" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                    <option value="">None — screening estimates only</option>
                    @foreach ($calibrations as $cal)
                        <option value="{{ $cal->id }}" {{ $cal->is_active ? 'selected' : '' }}>
                            {{ $cal->name }} v{{ $cal->version }}
                        </option>
                    @endforeach
                </select>
            </div>
        </div>

        <div>
            <label class="block text-sm font-medium text-gray-700">Marker Panel</label>
            <div class="mt-2 flex flex-wrap gap-3">
                @foreach (['16S', '12S', 'cytb', 'COI'] as $marker)
                    <label class="inline-flex items-center">
                        <input type="checkbox" name="marker_panel[]" value="{{ $marker }}" checked class="rounded border-gray-300 text-brand-600">
                        <span class="ml-1.5 text-sm text-gray-700">{{ $marker }}</span>
                    </label>
                @endforeach
            </div>
        </div>

        <details class="text-sm">
            <summary class="cursor-pointer text-gray-500 hover:text-gray-700">Advanced Options</summary>
            <div class="mt-3 grid sm:grid-cols-2 gap-4">
                <div>
                    <label class="block text-xs text-gray-600">Detection Threshold (w/w)</label>
                    <input type="number" name="detection_threshold" value="0.001" step="0.0001" min="0" max="1" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                </div>
                <div>
                    <label class="block text-xs text-gray-600">Bootstrap Resamples</label>
                    <input type="number" name="n_bootstrap" value="0" min="0" max="10000" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                </div>
                <div>
                    <label class="block text-xs text-gray-600">Read Mode</label>
                    <select name="read_mode" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                        <option value="auto">Auto-detect</option>
                        <option value="short">Short (Illumina)</option>
                        <option value="long">Long (Nanopore/PacBio)</option>
                    </select>
                </div>
            </div>
        </details>
    </div>

    <!-- Samples -->
    <div class="bg-white rounded-lg shadow p-6 space-y-4" x-data="{ samples: [{role:'sample',label:'',product_type:'',claimed_species:''}] }">
        <div class="flex justify-between items-center">
            <h2 class="text-lg font-semibold text-gray-800">Samples & Controls</h2>
            <button type="button" @click="samples.push({role:'sample',label:'',product_type:'',claimed_species:''})"
                class="text-sm text-brand-600 hover:text-brand-700 font-medium">+ Add Sample</button>
        </div>

        <template x-for="(s, i) in samples" :key="i">
            <div class="border border-gray-200 rounded-lg p-4 space-y-3">
                <div class="flex justify-between items-start">
                    <span class="text-sm font-medium text-gray-600" x-text="'Sample ' + (i + 1)"></span>
                    <button type="button" @click="samples.splice(i, 1)" x-show="samples.length > 1"
                        class="text-xs text-red-500 hover:text-red-700">Remove</button>
                </div>

                <div class="grid sm:grid-cols-3 gap-3">
                    <div>
                        <label class="block text-xs text-gray-500">Role</label>
                        <select :name="'samples['+i+'][role]'" required class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm">
                            <option value="sample">Test Sample</option>
                            <option value="positive_control">Positive Control</option>
                            <option value="negative_control">Negative Control (NTC)</option>
                            <option value="extraction_blank">Extraction Blank</option>
                        </select>
                    </div>
                    <div>
                        <label class="block text-xs text-gray-500">Label</label>
                        <input type="text" :name="'samples['+i+'][label]'" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm" placeholder="e.g. S001">
                    </div>
                    <div>
                        <label class="block text-xs text-gray-500">Product Type</label>
                        <input type="text" :name="'samples['+i+'][product_type]'" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm" placeholder="e.g. sausage">
                    </div>
                </div>

                <div class="grid sm:grid-cols-2 gap-3">
                    <div>
                        <label class="block text-xs text-gray-500">Claimed Species (comma-separated)</label>
                        <input type="text" :name="'samples['+i+'][claimed_species]'" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm" placeholder="e.g. Bos_taurus, Gallus_gallus">
                    </div>
                    <div>
                        <label class="block text-xs text-gray-500">Expected Species for Positive Control</label>
                        <input type="text" :name="'samples['+i+'][expected_species]'" class="mt-1 block w-full rounded-md border-gray-300 shadow-sm text-sm" placeholder="e.g. Bos_taurus">
                    </div>
                </div>

                <div class="grid sm:grid-cols-2 gap-3">
                    <div>
                        <label class="block text-xs text-gray-500">FASTQ File</label>
                        <input type="file" :name="'samples['+i+'][fastq]'" required accept=".fq,.fastq,.fq.gz,.fastq.gz" class="mt-1 block w-full text-sm text-gray-500 file:mr-3 file:py-1 file:px-3 file:rounded file:border-0 file:text-sm file:bg-brand-50 file:text-brand-700 hover:file:bg-brand-100">
                    </div>
                </div>
            </div>
        </template>
    </div>

    <div class="flex justify-end">
        <button type="submit" class="inline-flex items-center px-6 py-2.5 bg-brand-600 text-white text-sm font-medium rounded-md shadow hover:bg-brand-700 focus:outline-none focus:ring-2 focus:ring-brand-500">
            Start Analysis
        </button>
    </div>
</form>
@endsection
