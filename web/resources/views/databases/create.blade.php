@extends('layouts.app')
@section('title', 'Upload Reference Database')
@section('content')
<div class="max-w-3xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
    <h1 class="text-2xl font-bold text-gray-900 mb-6">Upload Reference Database</h1>
    <form method="POST" action="{{ route('databases.store') }}" enctype="multipart/form-data" class="bg-white rounded-lg shadow p-6 space-y-4">
        @csrf
        <input name="name" required placeholder="Name" class="block w-full rounded-md border-gray-300 text-sm">
        <input name="version" required placeholder="Version" class="block w-full rounded-md border-gray-300 text-sm">
        <label class="block text-sm">Database file (.db)<input type="file" name="db_file" required class="mt-1 block w-full"></label>
        <label class="block text-sm">Index file (.idx)<input type="file" name="index_file" required class="mt-1 block w-full"></label>
        <textarea name="notes" placeholder="Notes" class="block w-full rounded-md border-gray-300 text-sm"></textarea>
        <button class="bg-brand-600 text-white px-4 py-2 rounded-md text-sm">Save</button>
    </form>
</div>
@endsection
