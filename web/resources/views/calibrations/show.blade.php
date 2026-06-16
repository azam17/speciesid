@extends('layouts.app')
@section('title', $calibration->name)
@section('content')
<div class="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900">{{ $calibration->name }} v{{ $calibration->version }}</h1>
    <div class="mt-6 bg-white rounded-lg shadow p-6 text-sm space-y-2">
        <div><span class="text-gray-500">Hash:</span> <span class="font-mono">{{ $calibration->sha256_hash }}</span></div>
        <div><span class="text-gray-500">Reference database:</span> {{ $calibration->referenceDatabase->name ?? 'N/A' }}</div>
        <div><span class="text-gray-500">Locked:</span> {{ $calibration->is_locked ? 'Yes' : 'No' }}</div>
        <div><span class="text-gray-500">Notes:</span> {{ $calibration->notes ?: 'N/A' }}</div>
    </div>
</div>
@endsection
