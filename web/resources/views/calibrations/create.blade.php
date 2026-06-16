@extends('layouts.app')
@section('title', 'Upload Calibration')
@section('content')
<div class="max-w-3xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900 mb-6">Upload Calibration Profile</h1>
    <form method="POST" action="{{ route('calibrations.store') }}" enctype="multipart/form-data" class="bg-white rounded-lg shadow p-6 space-y-4">
        @csrf
        <input name="name" required placeholder="Name" class="block w-full rounded-md border-gray-300 text-sm">
        <input name="version" required placeholder="Version" class="block w-full rounded-md border-gray-300 text-sm">
        <select name="reference_database_id" required class="block w-full rounded-md border-gray-300 text-sm">
            <option value="">Select reference database...</option>
            @foreach($databases as $database)
                <option value="{{ $database->id }}">{{ $database->name }} v{{ $database->version }}</option>
            @endforeach
        </select>
        <label class="block text-sm">Calibration file (.cal)<input type="file" name="calibration_file" required class="mt-1 block w-full"></label>
        <textarea name="notes" placeholder="Notes" class="block w-full rounded-md border-gray-300 text-sm"></textarea>
        <button class="bg-brand-600 text-white px-4 py-2 rounded-md text-sm">Save</button>
    </form>
</div>
@endsection
