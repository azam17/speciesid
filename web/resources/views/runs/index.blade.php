@extends('layouts.app')
@section('title', 'Runs')
@section('content')
<div class="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <div class="flex items-center justify-between mb-6">
        <h1 class="text-2xl font-bold text-gray-900">Analysis Runs</h1>
        <a href="{{ route('upload.create') }}" class="text-sm bg-brand-600 text-white px-3 py-2 rounded-md">New Run</a>
    </div>
    <div class="bg-white shadow rounded-lg overflow-hidden">
        <table class="min-w-full divide-y divide-gray-200 text-sm">
            <thead class="bg-gray-50">
                <tr>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Run</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Owner</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Status</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">QC</th>
                    <th class="px-4 py-3 text-left text-xs font-medium text-gray-500 uppercase">Created</th>
                </tr>
            </thead>
            <tbody class="divide-y divide-gray-100">
                @forelse($runs as $run)
                    <tr>
                        <td class="px-4 py-3"><a class="text-brand-600 hover:underline" href="{{ route('runs.show', $run) }}">{{ $run->label ?: $run->uuid }}</a></td>
                        <td class="px-4 py-3 text-gray-600">{{ $run->user->name ?? 'N/A' }}</td>
                        <td class="px-4 py-3">{{ $run->status }}</td>
                        <td class="px-4 py-3">{{ $run->run_qc_status ?? 'N/A' }}</td>
                        <td class="px-4 py-3 text-gray-500">{{ $run->created_at?->format('Y-m-d H:i') }}</td>
                    </tr>
                @empty
                    <tr><td colspan="5" class="px-4 py-8 text-center text-gray-500">No runs yet.</td></tr>
                @endforelse
            </tbody>
        </table>
    </div>
    <div class="mt-4">{{ $runs->links() }}</div>
</div>
@endsection
