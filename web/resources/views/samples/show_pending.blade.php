@extends('layouts.app')
@section('title', $sample->label ?: $sample->sample_id)
@section('content')
<div class="max-w-3xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900">{{ $sample->label ?: $sample->sample_id }}</h1>
    <p class="text-sm text-gray-500 mt-1">Run: <a class="text-brand-600 hover:underline" href="{{ route('runs.show', $sample->run) }}">{{ $sample->run->label }}</a></p>
    <div class="mt-6 bg-white rounded-lg shadow p-6">
        <p class="text-gray-700">Results are not available yet.</p>
        <p class="text-sm text-gray-500 mt-2">Current run status: {{ $sample->run->status }}</p>
    </div>
</div>
@endsection
