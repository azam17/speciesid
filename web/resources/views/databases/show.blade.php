@extends('layouts.app')
@section('title', $database->name)
@section('content')
<div class="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900">{{ $database->name }} v{{ $database->version }}</h1>
    <div class="mt-6 bg-white rounded-lg shadow p-6 text-sm space-y-2">
        <div><span class="text-gray-500">Hash:</span> <span class="font-mono">{{ $database->sha256_hash }}</span></div>
        <div><span class="text-gray-500">Markers:</span> {{ implode(', ', $database->marker_panel ?? []) }}</div>
        <div><span class="text-gray-500">Species:</span> {{ $database->n_species }}</div>
        <div><span class="text-gray-500">Notes:</span> {{ $database->notes ?: 'N/A' }}</div>
    </div>
</div>
@endsection
