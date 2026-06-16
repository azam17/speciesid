@extends('layouts.app')
@section('title', 'Calibration Profiles')
@section('content')
<div class="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <div class="flex justify-between items-center mb-6">
        <h1 class="text-2xl font-bold text-gray-900">Calibration Profiles</h1>
        @can('create', App\Models\CalibrationProfile::class)
            <a href="{{ route('calibrations.create') }}" class="text-sm bg-brand-600 text-white px-3 py-2 rounded-md">Upload Calibration</a>
        @endcan
    </div>
    <div class="bg-white shadow rounded-lg overflow-hidden">
        <table class="min-w-full divide-y divide-gray-200 text-sm">
            <thead class="bg-gray-50"><tr><th class="px-4 py-3 text-left">Name</th><th class="px-4 py-3 text-left">Version</th><th class="px-4 py-3 text-left">Database</th><th class="px-4 py-3 text-left">Status</th><th></th></tr></thead>
            <tbody class="divide-y divide-gray-100">
                @forelse($calibrations as $calibration)
                    <tr><td class="px-4 py-3"><a class="text-brand-600 hover:underline" href="{{ route('calibrations.show', $calibration) }}">{{ $calibration->name }}</a></td><td class="px-4 py-3">{{ $calibration->version }}</td><td class="px-4 py-3">{{ $calibration->referenceDatabase->name ?? 'N/A' }}</td><td class="px-4 py-3">{{ $calibration->is_active ? 'Active' : 'Inactive' }}</td><td class="px-4 py-3 text-right">@can('activate', $calibration)<form method="POST" action="{{ route('calibrations.activate', $calibration) }}">@csrf<button class="text-brand-600">Activate</button></form>@endcan</td></tr>
                @empty
                    <tr><td colspan="5" class="px-4 py-8 text-center text-gray-500">No calibration profiles registered.</td></tr>
                @endforelse
            </tbody>
        </table>
    </div>
</div>
@endsection
